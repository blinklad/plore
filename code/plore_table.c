typedef struct plore_path_ref {
	char *Absolute;
	char *FilePart;
} plore_path_ref;

typedef struct plore_file_listing_info_create_result {
	b64 DidAlreadyExist;
	plore_file_listing_info_slot *Slot;
} plore_file_listing_info_create_result;

typedef struct plore_file_listing_desc {
	plore_file_node Type;
	plore_path_ref Path;
	plore_time LastModification;
	u64 Bytes;
} plore_file_listing_desc;


internal plore_file_listing_info_create_result
CreateFileInfo(plore_file_context *Context, plore_path *Path);

internal plore_file_listing_desc
ListingFromFile(plore_file *File) {
	plore_file_listing_desc Result = {
		.Type = File->Type,
		.Path = {
			.Absolute = File->Path.Absolute,
			.FilePart = File->Path.FilePart,
		},
		.LastModification = File->LastModification,
		.Bytes = File->Bytes,
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

internal plore_file_listing_info *
GetInfo(plore_file_context *Context, char *AbsolutePath) {
	plore_file_listing_info *Result = 0;
	plore_file_listing_info_slot *Slot = 0;
	u64 Hash = HashString(AbsolutePath);
	u64 Index = Hash % ArrayCount(Context->InfoSlots);
	Slot = Context->InfoSlots[Index];
	Assert(Slot);
	
	if (Slot->Allocated) {
		if (!StringsAreEqual(Slot->Info.Path.Absolute, AbsolutePath)) {
			for (u64 SlotCount = 0; SlotCount < Context->FileCount; SlotCount++) {
				Index = (Index + 1) % ArrayCount(Context->InfoSlots);
				Slot = Context->InfoSlots[Index];
				if (Slot->Allocated) {
					if (StringsAreEqual(Slot->Info.Path.Absolute, AbsolutePath)) {
						Result = &Slot->Info;
						break;
					}
				} else {
					break;
				}
			}
		} else {
			Result = &Slot->Info;
		}
	}
	
	if (Slot && Result) Assert(Slot->Allocated);
	return(Result);
}

internal void
RemoveFileInfo(plore_file_context *Context, plore_file_listing_info *Info) {
	u64 Hash = HashString(Info->Path.Absolute);
	u64 Index = Hash % ArrayCount(Context->InfoSlots);
	plore_file_listing_info_slot *Slot = Context->InfoSlots[Index];
	
	u64 SlotsChecked = 0;
	if (Slot->Allocated && !StringsAreEqual(Slot->Info.Path.Absolute, Info->Path.Absolute)) {
		Assert(Context->FileCount < ArrayCount(Context->InfoSlots));
		for (;;) {
			Index = (Index + 1) % ArrayCount(Context->InfoSlots);
			Slot = Context->InfoSlots[Index];
			SlotsChecked++;
			if (Slot->Allocated) {
				if (StringsAreEqual(Slot->Info.Path.Absolute, Info->Path.Absolute)) { // Move along.
					break;
				}
			}
			
			if (SlotsChecked == ArrayCount(Context->InfoSlots)) {
				return; // NOTE(Evan): Tried to delete something non-existent/unallocated.
			}
		}
	}
	
	Context->FileCount--;
	Slot->Info = (plore_file_listing_info) {0};
	Slot->Allocated = false;
}

typedef struct plore_file_listing_info_get_or_create_result {
	plore_file_listing_info *Info;
	b64 DidAlreadyExist;
} plore_file_listing_info_get_or_create_result;

internal plore_file_listing_info_get_or_create_result
GetOrCreateFileInfo(plore_file_context *Context, plore_path *Path) {
	plore_file_listing_info_get_or_create_result Result = {
		.Info = GetInfo(Context, Path->Absolute),
	};
	
	if (Result.Info) {
		Result.DidAlreadyExist = true;
	} else {
		plore_file_listing_info_create_result InfoResult = CreateFileInfo(Context, Path);
		Assert(!InfoResult.DidAlreadyExist);
		Result.Info = &InfoResult.Slot->Info;
	}
	return(Result);
}

internal plore_file_listing_info_create_result
CreateFileInfo(plore_file_context *Context, plore_path *Path) {
	plore_file_listing_info_create_result Result = {0};
	
	u64 Hash = HashString(Path->Absolute);
	u64 Index = Hash % ArrayCount(Context->InfoSlots);
	Result.Slot = Context->InfoSlots[Index];
	
	Assert(Result.Slot);
	if (Result.Slot->Allocated && !StringsAreEqual(Result.Slot->Info.Path.Absolute, Path->Absolute)) {
		Assert(Context->FileCount < ArrayCount(Context->InfoSlots));
		for (;;) {
			Index = (Index + 1) % ArrayCount(Context->InfoSlots);
			Result.Slot = Context->InfoSlots[Index];
			if (!Result.Slot->Allocated) { // Marked for deletion.
				Result.Slot->Allocated = true;
				break;
			} else if (StringsAreEqual(Result.Slot->Info.Path.Absolute, Path->Absolute)) { // Move along.
				Result.DidAlreadyExist = true;
				break;
			}
		}
	}
	
	// NOTE(Evan): Create file info if we need to!
	if (!Result.DidAlreadyExist) {
		Result.Slot->Allocated = true;
		Result.Slot->Info.Cursor = 0;
		Result.Slot->Info.Filter.State = TextFilterState_Show;
		Result.Slot->Info.Filter.TextCount = 0;
		
		StringCopy(Path->Absolute, Result.Slot->Info.Path.Absolute, PLORE_MAX_PATH);
		StringCopy(Path->FilePart, Result.Slot->Info.Path.FilePart, PLORE_MAX_PATH);
		Context->FileCount++;
	}
	
	if (Result.Slot) Assert(Result.Slot->Allocated);
	return(Result);
}
