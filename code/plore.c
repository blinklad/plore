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
} plore_state;

#include "plore_vimgui.c"

internal void
PrintDirectory(plore_directory_listing *Directory);

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
		State = PushStruct(Arena, plore_state);
		State->Initialized = true;
		State->Memory = PloreMemory;
		State->FileContext = PushStruct(Arena, plore_file_context);
		State->VimguiContext = PushStruct(Arena, plore_vimgui_context);
		
		SetupPlatformCode(PlatformAPI);
	}
	
	if (PloreInput->DLLWasReloaded) {
		SetupPlatformCode(PlatformAPI);
	}
	plore_file_context *FileContext = State->FileContext;
	keyboard_and_mouse Input = PloreInput->ThisFrame;
	
	Platform->GetCurrentDirectory(FileContext->CurrentDirectory.Name, ArrayCount(FileContext->CurrentDirectory.Name));
	directory_entry_result CurrentDirectory = Platform->GetDirectoryEntries(FileContext->CurrentDirectory.Name, 
																			FileContext->CurrentDirectory.Entries, 
																			ArrayCount(FileContext->CurrentDirectory.Entries));
	FileContext->CurrentDirectory.Count = CurrentDirectory.Count;
	if (!CurrentDirectory.Succeeded) {
		PrintLine("Could not get current directory.");
	}
	
	Assert(FileContext->CurrentDirectory.Cursor < FileContext->CurrentDirectory.Count);
	plore_file *CursorEntry = FileContext->CurrentDirectory.Entries + FileContext->CurrentDirectory.Cursor;
		
	if (CursorEntry->Type == PloreFileNode_Directory) {
		char *CursorDirectoryName = CursorEntry->AbsolutePath;
		directory_entry_result CursorDirectory = Platform->GetDirectoryEntries(CursorDirectoryName, 
																			   FileContext->CursorDirectory.Entries, 
																			   ArrayCount(FileContext->CursorDirectory.Entries));
		
		if (!CursorDirectory.Succeeded) {
			PrintLine("Could not get cursor directory.");
		}
		CStringCopy(CursorDirectory.Name, FileContext->CursorDirectory.Name, ArrayCount(FileContext->CursorDirectory.Name));
		FileContext->CursorDirectory.Count = CursorDirectory.Count;
	} else {
		Platform->DebugPrintLine("Cursor is on file %s", CursorEntry->AbsolutePath);
	}
	
	CStringCopy(FileContext->CurrentDirectory.Name, FileContext->ParentDirectory.Name, ArrayCount(FileContext->ParentDirectory.Name));
	
	Platform->PopPathNode(FileContext->ParentDirectory.Name, ArrayCount(FileContext->ParentDirectory.Name), false);
	directory_entry_result ParentDirectory = Platform->GetDirectoryEntries(FileContext->ParentDirectory.Name, 
																		   FileContext->ParentDirectory.Entries, 
																		   ArrayCount(FileContext->ParentDirectory.Entries));
	FileContext->ParentDirectory.Count = ParentDirectory.Count;
	if (!ParentDirectory.Succeeded) {
		PrintLine("Could not get parent directory.");
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
		
		plore_directory_listing *Directory = State->FileContext->Directories + Col;
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
	
	
#if 0
	for (u64 Dir = 0; Dir < ArrayCount(FileContext->Directories); Dir++) {
		PrintDirectory(FileContext->Directories + Dir);
	}
#endif
	return(State->VimguiContext->RenderList);
}

internal void
PrintDirectory(plore_directory_listing *Directory) {
	PrintLine("%s", Directory->Name);
	for (u64 File = 0; File < Directory->Count; File++) {
		plore_file *FileNode = Directory->Entries + File;
		PrintLine("\t%s", FileNode->Name);
	}
	PrintLine("");
}

