typedef struct plore_path_ref {
	char *Absolute;
	char *FilePart;
} plore_path_ref;

typedef struct plore_file_listing_cursor_create_result {
	b64 DidAlreadyExist;
	plore_file_listing_cursor_slot *Slot;
} plore_file_listing_cursor_create_result;

typedef struct plore_file_listing_desc {
	plore_file_node Type;
	plore_path_ref Path;
	plore_time LastModification;
} plore_file_listing_desc;


internal plore_file_listing_cursor_create_result
CreateCursor(plore_file_context *Context, plore_path *Path);

internal plore_file_listing_desc
ListingFromFile(plore_file *File) {
	plore_file_listing_desc Result = {
		.Type = File->Type,
		.Path = {
			.Absolute = File->Path.Absolute,
			.FilePart = File->Path.FilePart,
		},
		.LastModification = File->LastModification,
	};
	
	return(Result);
}

// NOTE(Evan): Is this needed?
internal plore_file_listing_desc
ListingFromDirectoryPath(plore_path *Path) {
	plore_file_listing_desc Result = {
		.Type = PloreFileNode_Directory,
		.Path = {
			.Absolute = Path->Absolute,
			.FilePart = Path->FilePart,
		},
	};
	
	return(Result);
}

internal plore_file_listing_cursor *
GetCursor(plore_file_context *Context, char *AbsolutePath) {
	plore_file_listing_cursor *Result = 0;
	plore_file_listing_cursor_slot *Slot = 0;
	u64 Hash = HashString(AbsolutePath);
	u64 Index = Hash % ArrayCount(Context->CursorSlots);
	Slot = Context->CursorSlots[Index];
	Assert(Slot);
	
	if (Slot->Allocated) {
		if (!CStringsAreEqual(Slot->Cursor.Path.Absolute, AbsolutePath)) {
			for (u64 SlotCount = 0; SlotCount < Context->FileCount; SlotCount++) {
				Index = (Index + 1) % ArrayCount(Context->CursorSlots);
				Slot = Context->CursorSlots[Index];
				if (Slot->Allocated) {
					if (CStringsAreEqual(Slot->Cursor.Path.Absolute, AbsolutePath)) {
						Result = &Slot->Cursor;
						break;
					}
				} else {
					break;
				}
			}
		} else {
			Result = &Slot->Cursor;
		}
	}
	
	if (Slot && Result) Assert(Slot->Allocated);
	return(Result);
}

internal void
RemoveCursor(plore_file_context *Context, plore_file_listing_cursor *Cursor) {
	u64 Hash = HashString(Cursor->Path.Absolute);
	u64 Index = Hash % ArrayCount(Context->CursorSlots);
	plore_file_listing_cursor_slot *Slot = Context->CursorSlots[Index];
	
	u64 SlotsChecked = 0;
	if (Slot->Allocated && !CStringsAreEqual(Slot->Cursor.Path.Absolute, Cursor->Path.Absolute)) {
		Assert(Context->FileCount < ArrayCount(Context->CursorSlots));
		for (;;) {
			Index = (Index + 1) % ArrayCount(Context->CursorSlots);
			Slot = Context->CursorSlots[Index];
			SlotsChecked++;
			if (Slot->Allocated) {
				if (CStringsAreEqual(Slot->Cursor.Path.Absolute, Cursor->Path.Absolute)) { // Move along.
					break;
				}
			}
			
			if (SlotsChecked == ArrayCount(Context->CursorSlots)) {
				return; // NOTE(Evan): Tried to delete something non-existent/unallocated.
			}
		}
	}
	
	Context->FileCount--;
	Slot->Cursor = (plore_file_listing_cursor) {0};
	Slot->Allocated = false;
}

typedef struct plore_file_listing_cursor_get_or_create_result {
	plore_file_listing_cursor *Cursor;
	b64 DidAlreadyExist;
} plore_file_listing_cursor_get_or_create_result;

internal plore_file_listing_cursor_get_or_create_result
GetOrCreateCursor(plore_file_context *Context, plore_path *Path) {
	plore_file_listing_cursor_get_or_create_result Result = {
		.Cursor = GetCursor(Context, Path->Absolute),
	};
	
	if (Result.Cursor) {
		Result.DidAlreadyExist = true;
	} else {
		plore_file_listing_cursor_create_result Cursor = CreateCursor(Context, Path);
		Assert(!Cursor.DidAlreadyExist);
		Result.Cursor = &Cursor.Slot->Cursor;
	}
	return(Result);
}

internal plore_file_listing_cursor_create_result
CreateCursor(plore_file_context *Context, plore_path *Path) {
	plore_file_listing_cursor_create_result Result = {0};
	
	u64 Hash = HashString(Path->Absolute);
	u64 Index = Hash % ArrayCount(Context->CursorSlots);
	Result.Slot = Context->CursorSlots[Index];
	
	Assert(Result.Slot);
	if (Result.Slot->Allocated && !CStringsAreEqual(Result.Slot->Cursor.Path.Absolute, Path->Absolute)) {
		Assert(Context->FileCount < ArrayCount(Context->CursorSlots));
		for (;;) {
			Index = (Index + 1) % ArrayCount(Context->CursorSlots);
			Result.Slot = Context->CursorSlots[Index];
			if (!Result.Slot->Allocated) { // Marked for deletion.
				Result.Slot->Allocated = true;
				break;
			} else if (CStringsAreEqual(Result.Slot->Cursor.Path.Absolute, Path->Absolute)) { // Move along.
				Result.DidAlreadyExist = true;
				break;
			}
		}
	}
	
	// NOTE(Evan): Make a directory Cursor if we need to!
	if (!Result.DidAlreadyExist) {
		Result.Slot->Allocated = true;
		Result.Slot->Cursor.Cursor = 0;
		
		CStringCopy(Path->Absolute, Result.Slot->Cursor.Path.Absolute, PLORE_MAX_PATH);
		CStringCopy(Path->FilePart, Result.Slot->Cursor.Path.FilePart, PLORE_MAX_PATH);
		Context->FileCount++;
	}
	
	if (Result.Slot) Assert(Result.Slot->Allocated);
	return(Result);
}
