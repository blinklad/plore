internal plore_file_listing *
GetListing(plore_file_context *Context, char *Name) {
	plore_file_listing *Result = 0;
	plore_file_listing_slot *Slot = 0;
	u64 Hash = HashString(Name);
	u64 Index = Hash % ArrayCount(Context->FileSlots);
	Slot = Context->FileSlots[Index];
	Assert(Slot);
	if (Slot->Allocated) {
		if (!CStringsAreEqual(Slot->Directory.Name, Name)) {
			for (;;) {
				Index = (Index + 1) % ArrayCount(Context->FileSlots);
				Slot = Context->FileSlots[Index];
				if (Slot->Allocated) {
					if (CStringsAreEqual(Slot->Directory.Name, Name)) {
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

typedef struct plore_file_listing_insert_result {
	b64 DidAlreadyExist;
	plore_file_listing_slot *Slot;
} plore_file_listing_insert_result;

internal plore_file_listing_insert_result
InsertListing(plore_file_context *Context, char *Name) {
	plore_file_listing_insert_result Result = {0};
	
	u64 Hash = HashString(Name);
	u64 Index = Hash % ArrayCount(Context->FileSlots);
	Result.Slot = Context->FileSlots[Index];
	
	Assert(Result.Slot);
	if (Result.Slot->Allocated && !CStringsAreEqual(Result.Slot->Directory.Name, Name)) {
		Assert(Context->FileCount < ArrayCount(Context->FileSlots));
		for (;;) {
			Index = (Index + 1) % ArrayCount(Context->FileSlots);
			Result.Slot = Context->FileSlots[Index];
			if (!Result.Slot->Allocated) {                           // Marked for deletion.
				Result.Slot->Allocated = true;
				break;
			} else if (CStringsAreEqual(Result.Slot->Directory.Name, Name)) { // Move along.
				Result.DidAlreadyExist = true;
				break;
			}
		}
	}
	
	if (!Result.DidAlreadyExist) {
		CStringCopy(Name, Result.Slot->Directory.Name, ArrayCount(Result.Slot->Directory.Entries));
		directory_entry_result CurrentDirectory = Platform->GetDirectoryEntries(Result.Slot->Directory.Name, 
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

