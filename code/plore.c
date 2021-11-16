#include "plore.h"
#include "plore_memory.c"
#include "plore_string.h"
#include "plore_vimgui.h"

global platform_api *Platform;
global platform_debug_print_line *PrintLine;
global platform_debug_print *Print;

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"

typedef struct plore_state {
	b32 Initialized;
	plore_memory *Memory;
	plore_file_context *FileContext;
	plore_vimgui_context *VimguiContext;
	memory_arena FrameArena;
	plore_font Font;
} plore_state;

#include "plore_vimgui.c"
#include "plore_table.c"

internal void
PrintDirectory(plore_file_listing *Directory);


internal void 
PlatformInit(platform_api *PlatformAPI) {
	Platform = PlatformAPI;
	PrintLine = PlatformAPI->DebugPrintLine;
	Print = PlatformAPI->DebugPrint;
}

u8 FontBuffer[1<<21];
u8 TempBitmap[512*512];

internal void 
FontInit(plore_state *Context)
{
	Context->Font = (plore_font) {
		.Height = 32.0f,
	};
	platform_readable_file FontFile = Platform->DebugOpenFile("data/fonts/Inconsolata-Light.ttf");
	platform_read_file_result TheFont = Platform->DebugReadEntireFile(FontFile, FontBuffer, ArrayCount(FontBuffer));
	Assert(TheFont.ReadSuccessfully);
	
	stbtt_BakeFontBitmap(FontBuffer, 0, Context->Font.Height, TempBitmap, 512, 512, 32, 96, Context->Font.Data); // no guarantee this fits!
	// can free ttf_buffer at this point
	Context->Font.Handle = Platform->CreateTextureHandle(TempBitmap, 512, 512);
	
	Platform->DebugCloseFile(FontFile);
}

PLORE_DO_ONE_FRAME(PloreDoOneFrame) {
	memory_arena *Arena = &PloreMemory->PermanentStorage;
	plore_state *State = (plore_state *)Arena->Memory;
	
	if (!State->Initialized) {
		PlatformInit(PlatformAPI);
		
		State = PushStruct(Arena, plore_state);
		State->Initialized = true;
		State->Memory = PloreMemory;
		State->FrameArena = SubArena(Arena, Megabytes(32), 16);
		
		State->FileContext = PushStruct(Arena, plore_file_context);
		State->FileContext->Current = PushStruct(Arena, plore_file_listing);
		for (u64 Dir = 0; Dir < ArrayCount(State->FileContext->FileSlots); Dir++) {
			State->FileContext->FileSlots[Dir] = PushStruct(Arena, plore_file_listing);
		}
		
		FontInit(State);
		State->VimguiContext = PushStruct(Arena, plore_vimgui_context);
		VimguiInit(State->VimguiContext, &State->Font);
			
	}
	
	ClearArena(&State->FrameArena);
	
	if (PloreInput->DLLWasReloaded) {
		PlatformInit(PlatformAPI);
	}
	plore_file_context *FileContext = State->FileContext;
	keyboard_and_mouse Input = PloreInput->ThisFrame;
	
	{
		char Buffer[PLORE_MAX_PATH];
		plore_get_current_directory_result Result = Platform->GetCurrentDirectory(Buffer, PLORE_MAX_PATH);
		FileContext->Current = GetOrInsertListing(FileContext, ListingFromDirectoryPath(Result.AbsolutePath, Result.FilePart));
		FileContext->InTopLevelDirectory = Platform->IsPathTopLevel(Buffer, PLORE_MAX_PATH);
	}
	
	plore_file_listing *Cursor = 0;
	if (FileContext->Current->Count != 0) {
		plore_file *CursorEntry = FileContext->Current->Entries + FileContext->Current->Cursor;
		
		Cursor = GetOrInsertListing(FileContext, ListingFromFile(CursorEntry));
	}
	
	plore_file_listing *Parent = 0;
	if (!FileContext->InTopLevelDirectory) { 
		// CLEANUP(Evan): Doesn't need to copy twice!
		plore_file_listing CurrentCopy = *FileContext->Current;
		plore_pop_path_node_result Result = Platform->PopPathNode(CurrentCopy.File.AbsolutePath, ArrayCount(CurrentCopy.File.AbsolutePath), false);
		
		Parent = GetOrInsertListing(FileContext, ListingFromDirectoryPath(Result.AbsolutePath, Result.FilePart));
	}
	
	// NOTE(Evan): GUI stuff.
	u64 Cols = 3;
	
	f32 FooterHeight = 50;
	f32 fCols = (f32) Cols;
	f32 PadX = 10.0f;
	f32 PadY = 10.0f;
	f32 W = ((PlatformAPI->WindowWidth  - 0)            - (fCols + 1) * PadX) / fCols;
	f32 H = ((PlatformAPI->WindowHeight - FooterHeight) - (1     + 1) * PadY) / 1;
	
	f32 X = 0;
	f32 Y = 0;
	
	VimguiBegin(State->VimguiContext, Input, V2(PlatformAPI->WindowWidth, PlatformAPI->WindowHeight));
	
	typedef struct plore_viewable_directory {
		plore_file_listing *File;
		b64 Focus;
	} plore_viewable_directory;
	
	plore_viewable_directory ViewDirectories[3] = {
		[0] = {
			.File = FileContext->InTopLevelDirectory ? 0 : Parent,
		},
		[1] = {
			.File = FileContext->Current,
			.Focus = true,
		},
		[2] = {
			.File = Cursor,
		},
	};
	
	if (Window(State->VimguiContext, (vimgui_window_desc) {
					   .Title = FileContext->Current->File.AbsolutePath,
					   .Rect = { .P = V2(0, 0), .Span = { PlatformAPI->WindowWidth, PlatformAPI->WindowHeight-FooterHeight } },
					   .Colour = V4(0.10, 0.1, 0.1, 1.0f),
				   })) {
		
		for (u64 Col = 0; Col < Cols; Col++) {
			v2 P      = V2(X, 0);
			v2 Span   = V2(W, H-12);
			v4 Colour = V4(0.2f, 0.2f, 0.2f, 1.0f);
			
			plore_viewable_directory *Directory = ViewDirectories + Col;
			plore_file_listing *Listing = Directory->File;
			if (!Listing) continue; /* Parent can be null, if we are currently looking at a top-level directory. */
			
			char *Title = Listing->File.FilePart;
			if (Window(State->VimguiContext, (vimgui_window_desc) {
							   .Title      = Title,
							   .Rect       = {P, Span}, 
							   .Colour     = Colour,
							   .Centered = true,
						       .ForceFocus = Directory->Focus })) {
				
				for (u64 Row = 0; Row < Listing->Count; Row++) {
					if (Listing->Cursor == Row) {
						PrintLine("`%s`'s cursor is on entry %d.", Title, Row);
					}
					if (Button(State->VimguiContext, (vimgui_button_desc) {
									   .Title = Listing->Entries[Row].FilePart,
									   .FillWidth = true,
									   .Centre = true,
									   .Colour = Listing->Cursor != Row ? (v4) {0} : V4(0.4, 0.4, 0.4, 0.1),
								   })) {
						Listing->Cursor = Row;
						PrintLine("Button %s was clicked!", Listing->Entries[Row].FilePart);
						if (Listing->Entries[Row].Type == PloreFileNode_Directory) {
							PrintLine("Changing Directory to %s", Listing->Entries[Row].AbsolutePath);
							Platform->SetCurrentDirectory(Listing->Entries[Row].AbsolutePath);
						}
					}
				}
				
				WindowEnd(State->VimguiContext);
			}
			
			X += W + PadX;
		}
		
		if (Cursor) {
			char CursorInfo[512] = {0};
			StringPrintSized(CursorInfo, 
							 ArrayCount(CursorInfo),
						     "[%s] %s 01-02-3", 
						     (Cursor->File.Type == PloreFileNode_Directory) ? "DIR" : "FILE", 
						     Cursor->File.FilePart);
			
			if (Button(State->VimguiContext, (vimgui_button_desc) {
						   .Title = CursorInfo,
						   .Rect = { 
							   .P = V2(0, PlatformAPI->WindowHeight + FooterHeight), .Span = V2(PlatformAPI->WindowWidth, FooterHeight + PadY)
						   },
					   })) {
			}
		}
		WindowEnd(State->VimguiContext);
	}
	
	
	VimguiEnd(State->VimguiContext);
	
	if (Input.HIsPressed) {
		char Buffer[PLORE_MAX_PATH] = {0};
		Platform->GetCurrentDirectory(Buffer, PLORE_MAX_PATH);
		
		Print("Moving up a directory, from %s ...", Buffer);
		Platform->PopPathNode(Buffer, PLORE_MAX_PATH, false);
		PrintLine("to %s ...", Buffer);
		
		Platform->SetCurrentDirectory(Buffer);
		
	} else if (Input.LIsPressed) {
		plore_file *CursorEntry = FileContext->Current->Entries + FileContext->Current->Cursor;
		
		if (CursorEntry->Type == PloreFileNode_Directory) {
			PrintLine("Moving down a directory, from %s to %s", FileContext->Current->File.AbsolutePath, CursorEntry->AbsolutePath);
			Platform->SetCurrentDirectory(CursorEntry->AbsolutePath);
		}
	}
	
	if (FileContext->Current->Count > 0) {
		if (Input.JIsPressed) {
			FileContext->Current->Cursor = (FileContext->Current->Cursor + 1) % FileContext->Current->Count;
		} else if (Input.KIsPressed) {
			if (FileContext->Current->Cursor == 0) {
				FileContext->Current->Cursor = FileContext->Current->Count-1;
			} else {
				FileContext->Current->Cursor -= 1;
			}
		}
	}
	
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
	PrintLine("%s", Directory->File.AbsolutePath);
	for (u64 File = 0; File < Directory->Count; File++) {
		plore_file *FileNode = Directory->Entries + File;
		PrintLine("\t%s", FileNode->AbsolutePath);
	}
	PrintLine("");
}

