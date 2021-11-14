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
		for (u64 Dir = 0; Dir < ArrayCount(State->FileContext->ViewDirectories); Dir++) {
			State->FileContext->ViewDirectories[Dir] = PushStruct(Arena, plore_file_listing);
		}
		
		for (u64 Dir = 0; Dir < ArrayCount(State->FileContext->FileSlots); Dir++) {
			State->FileContext->FileSlots[Dir] = PushStruct(Arena, plore_file_listing);
		}
		
		#if 0
		{
			plore_file_listing_slot *FirstSlot = State->FileContext->FileSlots[State->FileContext->DirectoryCount++];
			Platform->GetCurrentDirectory(FirstSlot->Directory.Name, 
										  ArrayCount(FirstSlot->Directory.Name));
			directory_entry_result CurrentDirectory = Platform->GetDirectoryEntries(FirstSlot->Directory.Name, 
																					FirstSlot->Directory.Entries, 
																					ArrayCount(FirstSlot->Directory.Entries));
			Assert(CurrentDirectory.Succeeded);
			FirstSlot->Directory.Count = CurrentDirectory.Count;
			InsertListing(State->FileContext, &FirstSlot->Directory);
			plore_file_listing *Result = GetListing(State->FileContext, "C:\\plore");
			int BreakHere = 5;
		}
		#endif
		
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
	}
	
	if (FileContext->Current->Count != 0) {
		plore_file *CursorEntry = FileContext->Current->Entries + FileContext->Current->Cursor;
		
		if (CursorEntry->Type == PloreFileNode_Directory) {
			char *CursorDirectoryName = CursorEntry->AbsolutePath;
			directory_entry_result CursorDirectory = Platform->GetDirectoryEntries(CursorDirectoryName, 
																				   FileContext->Cursor->Entries, 
																				   ArrayCount(FileContext->Cursor->Entries));
			
			if (!CursorDirectory.Succeeded) {
				PrintLine("Could not get cursor directory.");
			}
			CStringCopy(CursorDirectory.Name, FileContext->Cursor->Name, ArrayCount(FileContext->Cursor->Name));
			FileContext->Cursor->Count = CursorDirectory.Count;
		} else {
//			Platform->DebugPrintLine("Cursor is on file %s", CursorEntry->AbsolutePath);
		}
		
	}
	
	{
		CStringCopy(FileContext->Current->Name, FileContext->Parent->Name, ArrayCount(FileContext->Parent->Name));
		
		Platform->PopPathNode(FileContext->Parent->Name, ArrayCount(FileContext->Parent->Name), false);
		directory_entry_result ParentDirectory = Platform->GetDirectoryEntries(FileContext->Parent->Name, 
																			   FileContext->Parent->Entries, 
																			   ArrayCount(FileContext->Parent->Entries));
		FileContext->Parent->Count = ParentDirectory.Count;
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
	
	for (u64 Col = 0; Col < Cols; Col++) {
		v2 P      = V2(X + PadX, PadY);
		v2 Span   = V2(W, H);
		v4 Colour = V4(0.3f, 0.3f, 0.3f, 1.0f);
		
		plore_file_listing *Directory = State->FileContext->ViewDirectories[Col];
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

