#include "plore.h"
#include "plore_memory.c"
#include "plore_string.h"
#include "plore_vimgui.h"

global platform_api *Platform;
global platform_debug_print_line *PrintLine;
global platform_debug_print *Print;

typedef struct plore_state {
	b32 Initialized;
	plore_memory *Memory;
	plore_file_context *FileContext;
	plore_vimgui_context *VimguiContext;
	memory_arena FrameArena;
} plore_state;

#include "plore_vimgui.c"
#include "plore_table.c"

internal void
PrintDirectory(plore_file_listing *Directory);


internal void 
SetupPlatformCode(platform_api *PlatformAPI) {
	Platform = PlatformAPI;
	PrintLine = PlatformAPI->DebugPrintLine;
	Print = PlatformAPI->DebugPrint;
}

PLORE_DO_ONE_FRAME(PloreDoOneFrame) {
	memory_arena *Arena = &PloreMemory->PermanentStorage;
	plore_state *State = (plore_state *)Arena->Memory;
	
	if (!State->Initialized) {
		SetupPlatformCode(PlatformAPI);
		
		State = PushStruct(Arena, plore_state);
		State->Initialized = true;
		State->Memory = PloreMemory;
		State->FrameArena = SubArena(Arena, Megabytes(32), 16);
		
		State->FileContext = PushStruct(Arena, plore_file_context);
		State->FileContext->Current = PushStruct(Arena, plore_file_listing);
		for (u64 Dir = 0; Dir < ArrayCount(State->FileContext->FileSlots); Dir++) {
			State->FileContext->FileSlots[Dir] = PushStruct(Arena, plore_file_listing);
		}
		
		State->VimguiContext = PushStruct(Arena, plore_vimgui_context);
			
	}
	
	ClearArena(&State->FrameArena);
	
	if (PloreInput->DLLWasReloaded) {
		SetupPlatformCode(PlatformAPI);
	}
	plore_file_context *FileContext = State->FileContext;
	keyboard_and_mouse Input = PloreInput->ThisFrame;
	
	{
		char *Buffer = PushBytes(Arena, PLORE_MAX_PATH);
		Platform->GetCurrentDirectory(Buffer, PLORE_MAX_PATH);
		plore_file_listing *MaybeCurrentDir = GetListing(FileContext, Buffer);
		if (!MaybeCurrentDir) {
			PrintLine("Current directory entry not found. Our current: `%s`, actual current: `%s`.", FileContext->Current->Name, Buffer);
			plore_file_listing_insert_result ListResult = InsertListing(FileContext, Buffer);
			FileContext->Current = &ListResult.Slot->Directory;
		} else if (!CStringsAreEqual(FileContext->Current->Name, Buffer)) {
			PrintLine("Current directory exists (`%s`), but doesn't match what we think the current dir is (`%s`)", Buffer, FileContext->Current->Name);
			FileContext->Current = MaybeCurrentDir;
		}
		
		FileContext->InTopLevelDirectory = Platform->IsPathTopLevel(Buffer, PLORE_MAX_PATH);
		if (FileContext->InTopLevelDirectory) {
			PrintLine("We are now in a top-level directory, so there is no parent.");
		}
	}
	
	plore_file_listing Cursor = {0};
	if (FileContext->Current->Count != 0) {
		plore_file *CursorEntry = FileContext->Current->Entries + FileContext->Current->Cursor;
		
		if (CursorEntry->Type == PloreFileNode_Directory) {
			char *CursorDirectoryName = CursorEntry->AbsolutePath;
			directory_entry_result CursorDirectory = Platform->GetDirectoryEntries(CursorDirectoryName, 
																				   Cursor.Entries, 
																				   ArrayCount(Cursor.Entries));
			
			if (!CursorDirectory.Succeeded) {
				PrintLine("Could not get cursor directory.");
			}
			CStringCopy(CursorDirectory.Name, Cursor.Name, ArrayCount(Cursor.Name));
			Cursor.Count = CursorDirectory.Count;
		} else {
//			Platform->DebugPrintLine("Cursor is on file %s", CursorEntry->AbsolutePath);
		}
		
	}
	
	plore_file_listing Parent = {0};
	if (!FileContext->InTopLevelDirectory)
	{
		CStringCopy(FileContext->Current->Name, Parent.Name, ArrayCount(Parent.Name));
		
		Platform->PopPathNode(Parent.Name, ArrayCount(Parent.Name), false);
		directory_entry_result ParentDirectory = Platform->GetDirectoryEntries(Parent.Name, 
																			   Parent.Entries, 
																			   ArrayCount(Parent.Entries));
		Parent.Count = ParentDirectory.Count;
		if (!ParentDirectory.Succeeded) {
			PrintLine("Could not get parent directory.");
		}
	}
	
	// NOTE(Evan): GUI stuff.
	u64 Cols = 3;
	
	f32 fCols = (f32) Cols;
	f32 PadX = 10.0f;
	f32 PadY = 10.0f;
	f32 W = (PlatformAPI->WindowWidth  - (fCols + 1) * PadX) / fCols;
	f32 H = (PlatformAPI->WindowHeight - (1     + 1) * PadY) / 1;
	
	f32 X = 0;
	f32 Y = 0;
	
	VimguiBegin(State->VimguiContext, Input);
	
	plore_file_listing *ViewDirectories[3] = {
		FileContext->InTopLevelDirectory ? 0 : &Parent,
		FileContext->Current,
		&Cursor,
	};

	for (u64 Col = 0; Col < Cols; Col++) {
		v2 P      = V2(X + PadX, PadY);
		v2 Span   = V2(W, H);
		v4 Colour = V4(0.3f, 0.3f, 0.3f, 1.0f);
		
		plore_file_listing *Directory = ViewDirectories[Col];
		if (!Directory) continue; /* Parent can be null, if we are currently looking at a top-level directory. */
		
		char *Title = Directory->Name;
		if (Window(State->VimguiContext, (vimgui_window_desc) {
						   .Title = Title,
						   .Rect = {P, Span}, 
						   .Colour = Colour })) {
			for (u64 Row = 0; Row < Directory->Count; Row++) {
				if (Button(State->VimguiContext, (vimgui_button_desc) {
								   .Title = Directory->Entries[Row].Name,
								   .FillWidth = true,
								   .Centre = true })) {
					PrintLine("Button %s was clicked!", Directory->Entries[Row].Name);
					if (Directory->Entries[Row].Type == PloreFileNode_Directory) {
						PrintLine("Changing directory to %s", Directory->Entries[Row].AbsolutePath);
						Platform->SetCurrentDirectory(Directory->Entries[Row].AbsolutePath);
					}
				}
			}
		}
		
		X += W + PadX;
	}
	
	VimguiEnd(State->VimguiContext);
	
#if 1
	if (Input.HIsPressed) {
		char *Buffer = PushBytes(&State->FrameArena, PLORE_MAX_PATH);
		Platform->GetCurrentDirectory(Buffer, PLORE_MAX_PATH);
		
		Print("Moving up a directory, from %s ...", Buffer);
		Platform->PopPathNode(Buffer, PLORE_MAX_PATH, false);
		PrintLine("to %s ...", Buffer);
		
		Platform->SetCurrentDirectory(Buffer);
		
	}
#endif
	
#if 0
	for (u64 Dir = 0; Dir < ArrayCount(FileContext->ViewDirectories); Dir++) {
		PrintDirectory(FileContext->ViewDirectories + Dir);
	}
#endif
	
	
#if 0
	
#endif
	return(State->VimguiContext->RenderList);
}

internal void
PrintDirectory(plore_file_listing *Directory) {
	PrintLine("%s", Directory->Name);
	for (u64 File = 0; File < Directory->Count; File++) {
		plore_file *FileNode = Directory->Entries + File;
		PrintLine("\t%s", FileNode->Name);
	}
	PrintLine("");
}

