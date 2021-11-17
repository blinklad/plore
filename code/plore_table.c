typedef struct plore_file_listing_insert_result {
	b64 DidAlreadyExist;
	plore_file_listing_slot *Slot;
} plore_file_listing_insert_result;

typedef struct plore_file_listing_desc {
	plore_file_node Type;
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
	}
	
	if (Result.Slot) Assert(Result.Slot->Allocated);
	return(Result);
}

