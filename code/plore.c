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
SetupPlatformCode(platform_api *PlatformAPI) {
	Platform = PlatformAPI;
	PrintLine = PlatformAPI->DebugPrintLine;
	Print = PlatformAPI->DebugPrint;
}

PLORE_DO_ONE_FRAME(PloreDoOneFrame) {
	plore_state *State = (plore_state *)PloreMemory->PermanentStorage.Memory;
	if (!State->Initialized) {
		State->Initialized = true;
		State->Memory = PloreMemory;
		State->FileContext = PushStruct(&PloreMemory->PermanentStorage, plore_file_context);
		
		State->VimguiContext = PushStruct(&PloreMemory->PermanentStorage, plore_vimgui_context);
		SetupPlatformCode(PlatformAPI);
	}
	
	if (PloreInput->DLLWasReloaded) {
		SetupPlatformCode(PlatformAPI);
	}
	plore_file_context *FileContext = State->FileContext;
	
	keyboard_and_mouse Input = PloreInput->ThisFrame;
	
	Platform->GetCurrentDirectory(FileContext->CurrentDirectory.Name, ArrayCount(FileContext->CurrentDirectory.Name));
	
	CStringCopy(FileContext->CurrentDirectory.Name, FileContext->CurrentDirectory.Name, ArrayCount(FileContext->CurrentDirectory.Name));
	
	directory_entry_result CurrentDirectory = Platform->GetDirectoryEntries(FileContext->CurrentDirectory.Name, 
																			FileContext->CurrentDirectory.Entries, 
																			ArrayCount(FileContext->CurrentDirectory.Entries));
	FileContext->CurrentDirectory.Count = CurrentDirectory.Count;
	
	#if 1
	Platform->DebugPrintLine("%s:", FileContext->CurrentDirectory.Name);
	for (u64 File = 0; File < CurrentDirectory.Count; File++) {
		plore_file *FileNode = CurrentDirectory.Buffer + File;
		Platform->DebugPrintLine("\t%s", FileNode->FileNameInPath);
	}
	Platform->DebugPrintLine("");
	
	plore_file *CursorEntry = FileContext->CurrentDirectory.Entries + FileContext->CurrentDirectory.Cursor;
		
	if (CursorEntry->Type == PloreFileNode_Directory) {
		char *CursorDirectoryName = CursorEntry->AbsolutePath;
		directory_entry_result CursorDirectory = Platform->GetDirectoryEntries(CursorDirectoryName, 
																			   FileContext->CursorDirectory.Entries, 
																			   ArrayCount(FileContext->CursorDirectory.Entries));
		
		FileContext->CursorDirectory.Count = CursorDirectory.Count;
		Platform->DebugPrintLine("%s", CursorDirectoryName);
		for (u64 File = 0; File < CursorDirectory.Count; File++) {
			plore_file *FileNode = CursorDirectory.Buffer + File;
			Platform->DebugPrintLine("\t%s", FileNode->FileNameInPath);
		}
		Platform->DebugPrintLine("");
	} else {
		Platform->DebugPrintLine("Cursor is on file %s", CursorEntry->AbsolutePath);
	}
	
	CStringCopy(FileContext->CurrentDirectory.Name, FileContext->ParentDirectory.Name, ArrayCount(FileContext->ParentDirectory.Name));
	
	Platform->PopPathNode(FileContext->ParentDirectory.Name, ArrayCount(FileContext->ParentDirectory.Name));
	directory_entry_result ParentDirectory = Platform->GetDirectoryEntries(FileContext->ParentDirectory.Name, 
																		   FileContext->ParentDirectory.Entries, 
																		   ArrayCount(FileContext->ParentDirectory.Entries));
	
	FileContext->ParentDirectory.Count = ParentDirectory.Count;
	Platform->DebugPrintLine("%s", FileContext->ParentDirectory.Name);
	for (u64 File = 0; File < ParentDirectory.Count; File++) {
		plore_file *FileNode = ParentDirectory.Buffer + File;
		Platform->DebugPrintLine("\t%s", FileNode->FileNameInPath);
	}
	Platform->DebugPrintLine("");
	
	#endif
	
	u64 Cols = 3;
	
	f32 fCols = (f32) Cols;
	f32 PadX = 10.0f;
	f32 PadY = 10.0f;
	f32 W = (PlatformAPI->WindowWidth  - (fCols + 1) * PadX) / fCols;
	f32 H = (PlatformAPI->WindowHeight - (1     + 1) * PadY) / 1;
	
	f32 X = 0;
	f32 Y = 0;
	local char *Titles[] = {
		"Parent",
		"Current",
		"Cursor",
	};
	
	VimguiBegin(State->VimguiContext, Input);
	
	for (u64 Col = 0; Col < Cols; Col++) {
		v2 P      = V2(X + PadX, PadY);
		v2 Span   = V2(W, H);
		v4 Colour = V4(0.3f, 0.3f, 0.3f, 1.0f);
		
		if (WindowTitled(State->VimguiContext, Titles[Col], RectangleBottomLeftSpan(P, Span), Colour)) {
			plore_directory_listing *Directory = State->FileContext->Directories + Col;
			for (u64 Row = 0; Row < Directory->Count; Row++) {
				if (Button(State->VimguiContext, (vimgui_button_desc) {
								   .Title = Directory->Entries[Row].AbsolutePath,
								   .FillWidth = true,
								   .Centre = true,
						   })) {
				}
			}
		}
		
		X += W + PadX;
	}
	
	VimguiEnd(State->VimguiContext);
	return(State->VimguiContext->RenderList);
}