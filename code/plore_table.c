typedef struct plore_path_ref {
	char *Absolute;
	char *FilePart;
} plore_path_ref;

typedef struct plore_file_listing_insert_result {
	b64 DidAlreadyExist;
	plore_file_listing_slot *Slot;
} plore_file_listing_insert_result;

typedef struct plore_file_listing_desc {
	plore_file_node Type;
	plore_path_ref Path;
} plore_file_listing_desc;


internal plore_file_listing_insert_result
InsertListing(plore_file_context *Context, plore_file_listing_desc Desc);

internal plore_file_listing_desc
ListingFromFile(plore_file *File) {
	plore_file_listing_desc Result = {
		.Type = File->Type,
		.Path = {
			.Absolute = File->Path.Absolute,
			.FilePart = File->Path.FilePart,
		},
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

internal plore_file_listing *
GetListing(plore_file_context *Context, char *AbsolutePath) {
	plore_file_listing *Result = 0;
	plore_file_listing_slot *Slot = 0;
	u64 Hash = HashString(AbsolutePath);
	u64 Index = Hash % ArrayCount(Context->FileSlots);
	Slot = Context->FileSlots[Index];
	Assert(Slot);
	if (Slot->Allocated) {
		if (!CStringsAreEqual(Slot->Listing.File.Path.Absolute, AbsolutePath)) {
			for (u64 SlotCount = 0; SlotCount < Context->FileCount; SlotCount++) {
				Index = (Index + 1) % ArrayCount(Context->FileSlots);
				Slot = Context->FileSlots[Index];
				if (Slot->Allocated) {
					if (CStringsAreEqual(Slot->Listing.File.Path.Absolute, AbsolutePath)) {
						Result = &Slot->Listing;
						break;
					}
				} else {
					break;
				}
			}
		} else {
			Result = &Slot->Listing;
		}
	}
	
	if (Slot && Result) Assert(Slot->Allocated);
	return(Result);
}

typedef struct plore_file_listing_get_or_insert_result {
	plore_file_listing *Listing;
	b64 DidAlreadyExist;
} plore_file_listing_get_or_insert_result;

internal void
RemoveListing(plore_file_context *Context, plore_file_listing *Listing) {
	u64 Hash = HashString(Listing->File.Path.Absolute);
	u64 Index = Hash % ArrayCount(Context->FileSlots);
	plore_file_listing_slot *Slot = Context->FileSlots[Index];
	
	u64 SlotsChecked = 0;
	if (Slot->Allocated && !CStringsAreEqual(Slot->Listing.File.Path.Absolute, Listing->File.Path.Absolute)) {
		Assert(Context->FileCount < ArrayCount(Context->FileSlots));
		for (;;) {
			Index = (Index + 1) % ArrayCount(Context->FileSlots);
			Slot = Context->FileSlots[Index];
			SlotsChecked++;
			if (Slot->Allocated) {
				if (CStringsAreEqual(Slot->Listing.File.Path.Absolute, Listing->File.Path.Absolute)) { // Move along.
					break;
				}
			}
			
			if (SlotsChecked == ArrayCount(Context->FileSlots)) {
				return; // NOTE(Evan): Tried to delete something non-existent/unallocated.
			}
		}
	}
	
	Context->FileCount--;
	Slot->Listing = (plore_file_listing) {0};
	Slot->Allocated = false;
}

internal plore_file_listing_get_or_insert_result
GetOrInsertListing(plore_file_context *Context, plore_file_listing_desc Desc) {
	plore_file_listing_get_or_insert_result Result = {
		.Listing = GetListing(Context, Desc.Path.Absolute),
	};
	if (Result.Listing) {
		Result.DidAlreadyExist = true;
	} else {
		plore_file_listing_insert_result Listing = InsertListing(Context, Desc);
		Assert(!Listing.DidAlreadyExist);
		Result.Listing = &Listing.Slot->Listing;
	}
	return(Result);
}

internal plore_file_listing_insert_result
InsertListing(plore_file_context *Context, plore_file_listing_desc Desc) {
	plore_file_listing_insert_result Result = {0};
	
	u64 Hash = HashString(Desc.Path.Absolute);
	u64 Index = Hash % ArrayCount(Context->FileSlots);
	Result.Slot = Context->FileSlots[Index];
	
	Assert(Result.Slot);
	if (Result.Slot->Allocated && !CStringsAreEqual(Result.Slot->Listing.File.Path.Absolute, Desc.Path.Absolute)) {
		Assert(Context->FileCount < ArrayCount(Context->FileSlots));
		for (;;) {
			Index = (Index + 1) % ArrayCount(Context->FileSlots);
			Result.Slot = Context->FileSlots[Index];
			if (!Result.Slot->Allocated) { // Marked for deletion.
				Result.Slot->Allocated = true;
				break;
			} else if (CStringsAreEqual(Result.Slot->Listing.File.Path.Absolute, Desc.Path.Absolute)) { // Move along.
				Result.DidAlreadyExist = true;
				break;
			}
		}
	}
	
	// NOTE(Evan): Make a directory listing if we need to!
	if (!Result.DidAlreadyExist) {
		Result.Slot->Listing.File.Type = Desc.Type;
		CStringCopy(Desc.Path.Absolute, Result.Slot->Listing.File.Path.Absolute, PLORE_MAX_PATH);
		CStringCopy(Desc.Path.FilePart, Result.Slot->Listing.File.Path.FilePart, PLORE_MAX_PATH);
		if (Desc.Type == PloreFileNode_Directory) {
			directory_entry_result CurrentDirectory = Platform->GetDirectoryEntries(Result.Slot->Listing.File.Path.Absolute, 
																					Result.Slot->Listing.Entries, 
																					ArrayCount(Result.Slot->Listing.Entries));
			if (!CurrentDirectory.Succeeded) {
				PrintLine("Could not get current directory.");
			}
			Result.Slot->Listing.Count = CurrentDirectory.Count;
		} else { 
			// NOTE(Evan): GetDirectoryEntries does this for directories!
			Result.Slot->Listing.File.Extension = GetFileExtension(Desc.Path.FilePart).Extension; 
		}
		
		Context->FileCount++;
		Result.Slot->Allocated = true;
	}
	
	if (Result.Slot) Assert(Result.Slot->Allocated);
	return(Result);
}
