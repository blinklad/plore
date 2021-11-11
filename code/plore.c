#include "plore.h"
#include "plore_memory.c"

global platform_api *Platform;
global platform_debug_print_line *PrintLine;
global platform_debug_print *Print;

typedef struct plore_state {
	b32 Initialized;
	plore_file_context *FileContext;
} plore_state;

internal void 
SetupPlatformCode(platform_api *PlatformAPI) {
	Platform = PlatformAPI;
	PrintLine = PlatformAPI->DebugPrintLine;
	Print = PlatformAPI->DebugPrint;
}

PLORE_DO_ONE_FRAME(PloreDoOneFrame) {
	plore_render_list RenderList = {0};
	
	plore_state *State = (plore_state *)PloreMemory->PermanentStorage.Memory;
	if (!State->Initialized) {
		State->Initialized = true;
		
		State->FileContext = PushStruct(&PloreMemory->PermanentStorage, plore_file_context);
		SetupPlatformCode(PlatformAPI);
	}
	
	if (PloreInput->DLLWasReloaded) {
		SetupPlatformCode(PlatformAPI);
	}
	plore_file_context *FileContext = State->FileContext;
	
	keyboard_and_mouse Input = PloreInput->ThisFrame;
	if (Input.LIsPressed) {
	} else if (Input.HIsPressed) {
	} else if (Input.JIsPressed) {
	} else if (Input.KIsPressed) {
	}
	
	Platform->GetCurrentDirectory(FileContext->CurrentDirectory.Name, ArrayCount(FileContext->CurrentDirectory.Name));
	
	CStringCopy(FileContext->CurrentDirectory.Name, FileContext->CurrentDirectory.Name, ArrayCount(FileContext->CurrentDirectory.Name));
	
	directory_entry_result CurrentDirectory = Platform->GetDirectoryEntries(FileContext->CurrentDirectory.Name, 
																			FileContext->CurrentDirectory.Entries, 
																			ArrayCount(FileContext->CurrentDirectory.Entries));
	FileContext->CurrentDirectory.Count = CurrentDirectory.Count;
	
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
	
	Platform->DebugPrintLine("%s", FileContext->ParentDirectory.Name);
	for (u64 File = 0; File < ParentDirectory.Count; File++) {
		plore_file *FileNode = ParentDirectory.Buffer + File;
		Platform->DebugPrintLine("\t%s", FileNode->FileNameInPath);
	}
	Platform->DebugPrintLine("");
	
	u64 Cols = 5;
	const v4 Colours[] = {
		RED_V4,
		GREEN_V4,
		BLUE_V4,
		YELLOW_V4,
		MAGENTA_V4,
	};
	
	f32 PadX = 10.0f;
	f32 PadY = 10.0f;
	f32 W = PlatformAPI->WindowWidth  / (f32) Cols;
	f32 H = PlatformAPI->WindowHeight / 1.0f;
	
	f32 X = PadX;
	f32 Y = PadY;
	for (u64 Col = 0; Col < Cols; Col++) {
		RenderList.Quads[RenderList.QuadCount++] = (render_quad) {
			.Span   = V2(W-PadX, H - PadY),
			.P      = V2(X, PadY),
			.Colour = Colours[Col],
		};
		
		X += W + PadX;
	}
	
	return(RenderList);
}