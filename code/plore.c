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
	b64 Initialized;
	f64 DT;
	plore_memory *Memory;
	plore_file_context *FileContext;
	plore_vimgui_context *VimguiContext;
	plore_render_list *RenderList;
	
	memory_arena Arena;      // NOTE(Evan): Never freed.
	memory_arena FrameArena; // NOTE(Evan): Freed at the beginning of each frame.
	
	plore_font *Font;
} plore_state;

#include "plore_vimgui.c"
#include "plore_table.c"

internal void
PrintDirectory(plore_file_listing *Directory);

#if defined(PLORE_INTERNAL)
#include "plore_debug.c"
#endif

internal void 
PlatformInit(platform_api *PlatformAPI) {
	Platform = PlatformAPI;
	PrintLine = PlatformAPI->DebugPrintLine;
	Print = PlatformAPI->DebugPrint;
}

u8 FontBuffer[1<<21];
u8 TempBitmap[512*512];


// NOTE(Evan): This is hacked in here just so we can figure out what the program should do!
internal plore_font * 
FontInit(memory_arena *Arena, char *Path)
{
	plore_font *Result = PushStruct(Arena, plore_font);
	*Result = (plore_font) {
		.Fonts = {
			[0] = {
				.Height = 32.0f,
			}, 
			[1] = {
				.Height = 64.0f,
			},
		},
	};
	
	platform_readable_file FontFile = Platform->DebugOpenFile(Path);
	platform_read_file_result TheFont = Platform->DebugReadEntireFile(FontFile, FontBuffer, ArrayCount(FontBuffer));
	Assert(TheFont.ReadSuccessfully);
	
	for (u64 F = 0; F < 2; F++) {
		stbtt_BakeFontBitmap(FontBuffer, 0, Result->Fonts[F].Height, TempBitmap, 512, 512, 32, 96, Result->Fonts[F].Data); // No guarantee this fits!
		Result->Fonts[F].Handle = Platform->CreateTextureHandle(TempBitmap, 512, 512);
		
		Platform->DebugCloseFile(FontFile);
	}
	
	return(Result);
}

PLORE_DO_ONE_FRAME(PloreDoOneFrame) {
	memory_arena *Arena = &PloreMemory->PermanentStorage;
	plore_state *State = (plore_state *)Arena->Memory;
	
	if (!State->Initialized) {
		PlatformInit(PlatformAPI);
		
		State = PushStruct(Arena, plore_state);
		State->Initialized = true;
		State->Memory = PloreMemory;
		State->Arena = PloreMemory->PermanentStorage;
		State->FrameArena = SubArena(Arena, Megabytes(32), 16);
		
		State->FileContext = PushStruct(&State->Arena, plore_file_context);
		State->FileContext->Current = PushStruct(&State->Arena, plore_file_listing);
		for (u64 Dir = 0; Dir < ArrayCount(State->FileContext->FileSlots); Dir++) {
			State->FileContext->FileSlots[Dir] = PushStruct(&State->Arena, plore_file_listing);
		}
		
		State->Font = FontInit(&State->Arena, "data/fonts/Inconsolata-Light.ttf");
		
#if defined(PLORE_INTERNAL)
		DebugInit(State);
#endif
		
		State->RenderList = PushStruct(&State->Arena, plore_render_list);
		State->RenderList->Font = State->Font;
		
		State->VimguiContext = PushStruct(&State->Arena, plore_vimgui_context);
		VimguiInit(State->VimguiContext, State->RenderList);
			
	}
	
	
	ClearArena(&State->FrameArena);
	
#if defined(PLORE_INTERNAL)
	if (PloreInput->DLLWasReloaded) {
		PlatformInit(PlatformAPI);
		DebugInit(State);
	}
#endif
	
	plore_file_context *FileContext = State->FileContext;
	keyboard_and_mouse Input = PloreInput->ThisFrame;
	
	local f32 LastTime;
	State->DT = PloreInput->Time - LastTime;
	LastTime = PloreInput->Time;
	
	// CLEANUP(Evan): We Begin() here as the render list is shared for debug draw purposes right now.
	VimguiBegin(State->VimguiContext, Input, V2(PlatformAPI->WindowWidth, PlatformAPI->WindowHeight));
	
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
	
	if (Input.SlashIsPressed) {
		PrintLine("Slash pressed.");
	}
	
	local b64 WasPasted = 0;
	local b64 DoPaste = 0;
	
	local b64 WasUndone = 0;
	local b64 WasYanked = 0;
	local b64 DoYank = 0;
	local b64 UndoYank = 0;
	
	local plore_file_listing *Yanked = 0;
	if (Input.YIsPressed) {
		if (WasYanked) { // Yank
			WasYanked = false;
			DoYank = true;
		} else if (WasUndone) {
			UndoYank = true;
		} else {
			WasYanked = true;
		}
	} else if (Input.UIsPressed) { // Undo
		if (WasUndone || WasYanked || WasPasted) {
			WasUndone = false;
			WasPasted = false;
			WasYanked = false;
		} else {
			WasUndone = true;
		}
	} else if (Input.PIsPressed) { // Paste
		if (WasPasted) {
			WasPasted = false;
			DoPaste = true;
		} else {
			WasPasted = true;
		}
	} else {
		local u64 JCount = 0;
		if (Input.JIsDown) {
			WasYanked = false;
			WasPasted = false;
			DoYank = false;
			WasUndone = false;
			JCount++;
		} else {
			JCount = 0;
		}
		
		local u64 KCount = 0;
		if (Input.KIsDown) {
			WasYanked = false;
			WasPasted = false;
			DoYank = false;
			WasUndone = false;
			KCount++;
		} else {
			KCount = 0;
		}
		
		if (FileContext->Current->Count > 0) {
			if (Input.JIsPressed || JCount > 20) {
				FileContext->Current->Cursor = (FileContext->Current->Cursor + 1) % FileContext->Current->Count;
				if (JCount) JCount = 10;
			} else if (Input.KIsPressed || KCount > 20) {
				if (KCount) KCount = 10;
				if (FileContext->Current->Cursor == 0) {
					FileContext->Current->Cursor = FileContext->Current->Count-1;
				} else {
					FileContext->Current->Cursor -= 1;
				}
			}
		}
	
	}
	
	
	
	
	{
		char Buffer[PLORE_MAX_PATH];
		plore_get_current_directory_result Result = Platform->GetCurrentDirectory(Buffer, PLORE_MAX_PATH);
		FileContext->Current = GetOrInsertListing(FileContext, ListingFromDirectoryPath(Result.AbsolutePath, Result.FilePart)).Listing;
		FileContext->InTopLevelDirectory = Platform->IsPathTopLevel(Buffer, PLORE_MAX_PATH);
	}
	
	plore_file_listing *Cursor = 0;
	if (FileContext->Current->Count != 0) {
		plore_file *CursorEntry = FileContext->Current->Entries + FileContext->Current->Cursor;
		
		Cursor = GetOrInsertListing(FileContext, ListingFromFile(CursorEntry)).Listing;
	}
	
	plore_file_listing *Parent = 0;
	if (!FileContext->InTopLevelDirectory) { 
		// CLEANUP(Evan): Doesn't need to copy twice!
		plore_file_listing CurrentCopy = *FileContext->Current;
		plore_pop_path_node_result Result = Platform->PopPathNode(CurrentCopy.File.AbsolutePath, ArrayCount(CurrentCopy.File.AbsolutePath), false);
		
		plore_file_listing_get_or_insert_result ParentResult = GetOrInsertListing(FileContext, ListingFromDirectoryPath(Result.AbsolutePath, Result.FilePart));
		
		// Make sure the parent's cursor is set to the current directory.
		// The parent's row will be invalid on plore startup.
		Parent = ParentResult.Listing;
		if (!ParentResult.DidAlreadyExist) {
			for (u64 Row = 0; Row < Parent->Count; Row++) {
				plore_file *File = Parent->Entries + Row;
				if (File->Type != PloreFileNode_Directory) continue;
				
				if (CStringsAreEqual(File->AbsolutePath, FileContext->Current->File.AbsolutePath)) {
					DrawText("`%s`'s cursor has been set to %d (`%s`)", 
							 Parent->File.FilePart,
							 Row,
							 File->AbsolutePath);
					Parent->Cursor = Row;
					break;
				}
			}
														  
		}
		
	}
	
	//
	// NOTE(Evan): Vim command processing
	//
	if (DoYank) {
		if (Cursor && !Cursor->IsYanked) {
			if (Yanked) Yanked->IsYanked = false;
			Yanked = Cursor;
			Cursor->IsYanked = true;
			DrawText("Yanked %s!", Cursor->File.FilePart);
		}
		DoYank = false;
	} else if (UndoYank) {
		if (Yanked) {
			Yanked->IsYanked = false;
			DrawText("Unyanked %s!", Yanked->File.FilePart);
			Yanked = 0;
		}
		UndoYank = false;
	} else if (DoPaste) {
		if (Yanked) {
			Yanked->IsYanked = false;
			DrawText("'Pasted' %s!", Yanked->File.FilePart);
			Yanked = 0;
		}
		DoPaste = false;
	}
	
	
	
	//
	// NOTE(Evan): GUI stuff.
	//
	u64 Cols = 3;
	
	f32 FooterHeight = 50;
	f32 fCols = (f32) Cols;
	f32 PadX = 10.0f;
	f32 PadY = 10.0f;
	f32 W = ((PlatformAPI->WindowWidth  - 0)            - (fCols + 1) * PadX) / fCols;
	f32 H = ((PlatformAPI->WindowHeight - FooterHeight) - (1     + 1) * PadY) / 1;
	
	f32 X = 0;
	f32 Y = 0;
	
	
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
			v2 Span   = V2(W, H-22);
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
					v4 Colour = {0};
					plore_file_listing *RowEntry = GetListing(FileContext, Listing->Entries[Row].AbsolutePath);
					if (Listing->Cursor == Row) {
						Colour = V4(0.4, 0.4, 0.4, 0.1);
					} else if (RowEntry && RowEntry->IsYanked) {
						Colour = V4(0.3, 0.3, 0.4, 0.45);
					}
					if (Button(State->VimguiContext, (vimgui_button_desc) {
									   .Title = Listing->Entries[Row].FilePart,
									   .FillWidth = true,
									   .Centre = true,
									   .Colour = Colour,
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
			if (WasYanked || DoYank) {
				if (WasYanked) {
					StringPrintSized(CursorInfo, 
									 ArrayCount(CursorInfo),
									 "y");
				} 
			} else {
				StringPrintSized(CursorInfo, 
								 ArrayCount(CursorInfo),
							     "[%s] %s 01-02-3", 
							     (Cursor->File.Type == PloreFileNode_Directory) ? "DIR" : "FILE", 
							     Cursor->File.FilePart);
			}
			
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
	
#if defined(PLORE_INTERNAL)
	FlushText();
#endif
	
	VimguiEnd(State->VimguiContext);
	
	
	
	// NOTE(Evan): Right now, we copy this out. We may not want to in the future(tm), even if it is okay now.
	return(*State->VimguiContext->RenderList);
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

