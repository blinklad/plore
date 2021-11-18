typedef struct plore_file_listing_insert_result {
	b64 DidAlreadyExist;
	plore_file_listing_slot *Slot;
} plore_file_listing_insert_result;

typedef struct plore_file_listing_desc {
	plore_file_node Type;
	plore_file_listing_metadata Meta;
	char *AbsolutePath;
	char *FilePart;
} plore_file_listing_desc;

internal plore_file_listing_insert_result
InsertListing(plore_file_context *Context, plore_file_listing_desc Desc);

internal plore_file_listing_desc
ListingFromFile(plore_file *File) {
	plore_file_listing_desc Result = {
		.Type = File->Type,
		.AbsolutePath = File->AbsolutePath,
		.FilePart = File->FilePart,
	};
	
	return(Result);
}

internal plore_file_listing_desc
ListingFromDirectoryPath(char *AbsolutePath, char *FilePart) {
	plore_file_listing_desc Result = {
		.Type = PloreFileNode_Directory,
		.AbsolutePath = AbsolutePath,
		.FilePart = FilePart,
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
		if (!CStringsAreEqual(Slot->Directory.File.AbsolutePath, AbsolutePath)) {
			for (;;) {
				Index = (Index + 1) % ArrayCount(Context->FileSlots);
				Slot = Context->FileSlots[Index];
				if (Slot->Allocated) {
					if (CStringsAreEqual(Slot->Directory.File.AbsolutePath, AbsolutePath)) {
						Result = &Slot->Directory;
						break;
					}
				} else {
					break;
				}
			}
		} else {
			Result = &Slot->Directory;
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
	u64 Hash = HashString(Listing->File.AbsolutePath);
	u64 Index = Hash % ArrayCount(Context->FileSlots);
	plore_file_listing_slot *Slot = Context->FileSlots[Index];
	
	u64 SlotsChecked = 0;
	if (Slot->Allocated && !CStringsAreEqual(Slot->Directory.File.AbsolutePath, Listing->File.AbsolutePath)) {
		Assert(Context->FileCount < ArrayCount(Context->FileSlots));
		for (;;) {
			Index = (Index + 1) % ArrayCount(Context->FileSlots);
			Slot = Context->FileSlots[Index];
			SlotsChecked++;
			if (Slot->Allocated) {
				if (CStringsAreEqual(Slot->Directory.File.AbsolutePath, Listing->File.AbsolutePath)) { // Move along.
					break;
				}
			}
			
			if (SlotsChecked == ArrayCount(Context->FileSlots)) return; // NOTE(Evan): Tried to delete something non-existent/unallocated.
		}
	}
	
	Slot->Directory = (plore_file_listing) {0};
	Slot->Allocated = false;
}

internal plore_file_listing_get_or_insert_result
GetOrInsertListing(plore_file_context *Context, plore_file_listing_desc Desc) {
	plore_file_listing_get_or_insert_result Result = {
		.Listing = GetListing(Context, Desc.AbsolutePath),
	};
	if (Result.Listing) {
		Result.DidAlreadyExist = true;
	} else {
		plore_file_listing_insert_result Listing = InsertListing(Context, Desc);
		Assert(!Listing.DidAlreadyExist);
		Result.Listing = &Listing.Slot->Directory;
	}
	return(Result);
}

internal plore_file_listing_insert_result
InsertListing(plore_file_context *Context, plore_file_listing_desc Desc) {
	plore_file_listing_insert_result Result = {0};
	
	u64 Hash = HashString(Desc.AbsolutePath);
	u64 Index = Hash % ArrayCount(Context->FileSlots);
	Result.Slot = Context->FileSlots[Index];
	
	Assert(Result.Slot);
	if (Result.Slot->Allocated && !CStringsAreEqual(Result.Slot->Directory.File.AbsolutePath, Desc.AbsolutePath)) {
		Assert(Context->FileCount < ArrayCount(Context->FileSlots));
		for (;;) {
			Index = (Index + 1) % ArrayCount(Context->FileSlots);
			Result.Slot = Context->FileSlots[Index];
			if (!Result.Slot->Allocated) { // Marked for deletion.
				Result.Slot->Allocated = true;
				break;
			} else if (CStringsAreEqual(Result.Slot->Directory.File.AbsolutePath, Desc.AbsolutePath)) { // Move along.
				Result.DidAlreadyExist = true;
				break;
			}
		}
	}
	
	if (!Result.DidAlreadyExist) {
		Result.Slot->Directory.File.Type = Desc.Type;
		CStringCopy(Desc.AbsolutePath, Result.Slot->Directory.File.AbsolutePath, PLORE_MAX_PATH);
		CStringCopy(Desc.FilePart, Result.Slot->Directory.File.FilePart, PLORE_MAX_PATH);
		directory_entry_result CurrentDirectory = Platform->GetDirectoryEntries(Result.Slot->Directory.File.AbsolutePath, 
																				Result.Slot->Directory.Entries, 
																				ArrayCount(Result.Slot->Directory.Entries));
		if (!CurrentDirectory.Succeeded) {
			PrintLine("Could not get current directory.");
		}
		Result.Slot->Directory.Count = CurrentDirectory.Count;
		Result.Slot->Allocated = true;
		Result.Slot->Directory.Meta = Desc.Meta;
	}
	
	if (Result.Slot) Assert(Result.Slot->Allocated);
	return(Result);
}

internal b64
UpdateListingName(plore_file_context *Context, plore_file OldFile, plore_file NewFile) {
	b64 DidChange = false;
	plore_file_listing *OldListing = GetListing(Context, OldFile.AbsolutePath);
	if (OldListing) {
		// TODO(Evan): Maybe we don't store the metadata with the file, and instead keep a list of yanked files?
		plore_file_listing_metadata OldMeta = OldListing->Meta;
		RemoveListing(Context, OldListing);
		plore_file_listing_insert_result InsertResult = InsertListing(Context, (plore_file_listing_desc) {
																				  .Type = NewFile.Type,
																				  .AbsolutePath = NewFile.AbsolutePath,
																				  .FilePart = NewFile.FilePart,
																				  .Meta = OldMeta,
																			  });
		
		if (Context->Current == OldListing) Context->Current = &InsertResult.Slot->Directory;
	}
		
	// NOTE(Evan): Update the context's current if required.
	// Unless the caller wants to?
	return(DidChange);
}
