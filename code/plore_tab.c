internal plore_tab *
GetCurrentTab(plore_state *State) {
	Assert(State->TabCurrent < ArrayCount(State->Tabs));
	plore_tab *Result = State->Tabs + State->TabCurrent;
	
	return(Result);
}


internal void
InitTab(plore_state *State, plore_tab *Tab) {
	Assert(!Tab->Active);
	Assert(!Tab->Arena.BytesUsed);
	
	State->TabCount++;
	Tab->Active = true;
	
	Tab->FileContext = PushStruct(&Tab->Arena, plore_file_context);
	
	// NOTE(Evan): Don't show hidden files by default!
	Tab->FilterState = PushStruct(&Tab->Arena, plore_file_filter_state);
	Tab->FilterState->HideMask.HiddenFiles = true; 
	
	Tab->DirectoryState = PushStruct(&Tab->Arena, plore_current_directory_state);
	for (u64 Dir = 0; Dir < ArrayCount(Tab->FileContext->InfoSlots); Dir++) {
		Tab->FileContext->InfoSlots[Dir] = PushStruct(&Tab->Arena, plore_file_listing_info);
	}
	
	Tab->FileContext->Selected = MapInit(&Tab->Arena, sizeof(plore_path), 0, 256);
	Tab->FileContext->Yanked = MapInit(&Tab->Arena, sizeof(plore_path), 0, 256);
}

internal void
ClearTab(plore_state *State, u64 TabIndex) {
	Assert(TabIndex < ArrayCount(State->Tabs));
	Assert(State->TabCount > 1);
	
	State->TabCount--;
	plore_tab *Tab = State->Tabs + TabIndex;
	
	memory_arena Arena = Tab->Arena;
	ZeroArena(&Arena);
	*Tab = ClearStruct(plore_tab);
	Tab->Arena = Arena;
}

internal b64
SetCurrentTab(plore_state *State, u64 NewTab) {
	b64 Result = false;
	
	Assert(NewTab < ArrayCount(State->Tabs));
	plore_tab *Tab = State->Tabs + NewTab;
	plore_tab *Previous = State->Tabs + State->TabCurrent;
	if (Previous != Tab) {
		if (!Tab->Active) InitTab(State, Tab);
		
		Result = true;
		
		State->TabCurrent = NewTab;
		Platform->SetCurrentDirectory(Tab->CurrentDirectory.Absolute);
		
		//
		// NOTE(Evan): Setting a new tab will always be a frame late.
		// Otherwise, if a Button() or Window() call stores a pointer to a plore_file, Synchronize() will change the underlying
		// backing store for that string - i.e., we're fucked!
		// This could be solved in other ways, but it's not worth the complexity for now. Just render at least 60fps.
		//
		//SynchronizeCurrentDirectory(Tab);		
		
	}
	
	return(Result);
}
