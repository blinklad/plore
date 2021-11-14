internal plore_directory_listing *
GetListing(plore_file_context *Context, char *Name) {
	plore_directory_listing *Result = 0;
	plore_directory_listing_slot *Slot = 0;
	u64 Hash = HashString(Name);
	u64 Index = Hash % ArrayCount(Context->DirectorySlots);
	Slot = Context->DirectorySlots[Index];
	Assert(Slot);
	if (Slot->Allocated) {
		if (!CStringsAreEqual(Slot->Directory.Name, Name)) {
			for (;;) {
				Index = (Index + 1) % ArrayCount(Context->DirectorySlots);
				Slot = Context->DirectorySlots[Index];
				if (Slot->Allocated && CStringsAreEqual(Slot->Directory.Name, Name)) {
					Result = &Slot->Directory;
					break;
				}
			}
		} else {
			Result = &Slot->Directory;
		}
	}
	
	return(Result);
}

internal b64
InsertListing(plore_file_context *Context, plore_directory_listing *Listing) {
	b64 DidAlreadyExist = false;
	plore_directory_listing_slot *Slot = 0;
	
	u64 Hash = HashString(Listing->Name);
	u64 Index = Hash % ArrayCount(Context->DirectorySlots);
	Slot = Context->DirectorySlots[Index];
	
	Assert(Slot);
	if (Slot->Allocated && !CStringsAreEqual(Slot->Directory.Name, Listing->Name)) {
		Assert(Context->DirectoryCount < ArrayCount(Context->DirectorySlots));
		for (;;) {
			Index = (Index + 1) % ArrayCount(Context->DirectorySlots);
			Slot = Context->DirectorySlots[Index];
			if (!Slot->Allocated) {                           // Marked for deletion.
				Slot->Allocated = true;
				break;
			} else if (CStringsAreEqual(Slot->Directory.Name, Listing->Name)) { // Move along.
				DidAlreadyExist = true;
				break;
			}
		}
	}
	
	if (!DidAlreadyExist) {
		Slot->Directory = *Listing;
		Slot->Allocated = true;
	}
	
	return(DidAlreadyExist);
}

