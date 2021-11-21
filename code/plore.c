#include "plore.h"
#include "plore_memory.c"
#include "plore_string.h"
#include "plore_vimgui.h"
#include "plore_vim.h"

global platform_api *Platform;
global platform_debug_print_line *PrintLine;
global platform_debug_print *Print;

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"

typedef enum interact_state {
	InteractState_FileExplorer,
	InteractState_CommandHistory,
	
	_InteractState_ForceU64 = 0xFFFFFFFF,
} interact_state;
typedef struct plore_state {
	b64 Initialized;
	f64 DT;
	interact_state InteractState;
	
	plore_memory *Memory;
	plore_file_context *FileContext;
	plore_vim_context *VimContext;
	plore_vimgui_context *VimguiContext;
	plore_render_list *RenderList;
	
	memory_arena Arena;      // NOTE(Evan): Never freed.
	memory_arena FrameArena; // NOTE(Evan): Freed at the beginning of each frame.
	
	plore_font *Font;
} plore_state;

#include "plore_vim.c"
#include "plore_vimgui.c"
#include "plore_table.c"

internal void
PrintDirectory(plore_file_listing *Directory);

plore_inline b64
IsYanked(plore_file_context *Context, plore_file_listing *Yankee);

plore_inline b64
IsSelected(plore_file_context *Context, plore_file_listing *Selectee);

internal void
ToggleSelected(plore_file_context *Context, plore_file_listing *Selectee);


typedef struct plore_current_directory_state {
	plore_file_listing *Current;
	plore_file_listing *Parent;
	plore_file_listing *Cursor;
} plore_current_directory_state;

internal plore_current_directory_state
SynchronizeCurrentDirectory(plore_state *State);

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
FontInit(memory_arena *Arena, char *Path) {
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
		
		State->VimContext = PushStruct(&State->Arena, plore_vim_context);
		
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
	VimguiBegin(State->VimguiContext, Input, PlatformAPI->WindowDimensions);
	
	plore_current_directory_state DirectoryState = SynchronizeCurrentDirectory(State);
	
	local char Commands[32] = {0};
	local char YankCommand[] = { 'y', 'y', };
	local char PasteCommand[] = { 'p', 'p', };
	local char ClearYankCommand[] = { 'u', 'y', };
	local char MoveLeftCommand[] = { 'h' };
	local char MoveRightCommand[] = { 'l' };
	local char MoveUpCommand[] = { 'k' };
	local char MoveDownCommand[] = { 'j' };
	local char SelectCommand[] = { ' ' };
	
	local u64 MaxCommandCount = 2;
	local u64 CommandCount = 0;
	
	local b64 AteCommands = 0;
	
	
	// NOTE(Evan): Buffered input.
	local u64 JCount = 0;
	local u64 KCount = 0;
	if (Input.OIsPressed) {
		State->InteractState = ToggleFlag(State->InteractState, InteractState_CommandHistory);
	}
	
	if (Input.JIsDown) JCount++;
	else JCount = 0;
	if (JCount > 20) JCount = 10;
	
	if (Input.KIsDown) KCount++;
	else KCount = 0;
	if (KCount > 20) KCount = 10;
	
	
	
	b64 DidInput = true;
	if (Input.YIsPressed) {
		Commands[CommandCount++] = 'y';
	} else if (Input.UIsPressed) { // Undo
		Commands[CommandCount++] = 'u';
	} else if (Input.PIsPressed) { // Paste
		Commands[CommandCount++] = 'p';
	} else if (Input.SpaceIsPressed) { // Select
		Commands[CommandCount++] = ' '; // TODO(Evan): Command tokens?
	} else if (Input.JIsPressed || JCount == 20) {
		Commands[CommandCount++] = 'j';
	} else if (Input.KIsPressed || KCount == 20) {
		Commands[CommandCount++] = 'k';
	} else if (Input.HIsPressed) {
		Commands[CommandCount++] = 'h';
	} else if (Input.LIsPressed) {
		Commands[CommandCount++] = 'l';
	} else {
		DidInput = false;
	}
	
	b64 DoSelect = MemoryCompare(Commands, SelectCommand, ArrayCount(SelectCommand)) == 0;
	b64 DoMoveLeft = MemoryCompare(Commands, MoveLeftCommand, ArrayCount(MoveLeftCommand)) == 0;
	b64 DoMoveRight = MemoryCompare(Commands, MoveRightCommand, ArrayCount(MoveRightCommand)) == 0;
	b64 DoMoveUp = MemoryCompare(Commands, MoveUpCommand, ArrayCount(MoveUpCommand)) == 0;
	b64 DoMoveDown = MemoryCompare(Commands, MoveDownCommand, ArrayCount(MoveDownCommand)) == 0;
	b64 DoYank = MemoryCompare(Commands, YankCommand, ArrayCount(YankCommand)) == 0;
	b64 DoPaste = MemoryCompare(Commands, PasteCommand, ArrayCount(PasteCommand)) == 0;
	b64 ClearYank = MemoryCompare(Commands, ClearYankCommand, ArrayCount(ClearYankCommand)) == 0;
	b64 HasCommand = DoSelect || DoYank || DoPaste || ClearYank || DoMoveLeft || DoMoveRight || DoMoveUp || DoMoveDown;
	
	
	//
	// NOTE(Evan): Vim command processing
	//
	if (HasCommand) {
		switch (State->InteractState) {
			case (InteractState_FileExplorer): {
				if (DoYank) {
					u64 YankeeCount = 0;
					if (FileContext->SelectedCount) { // Yank selection if there is any.
						MemoryCopy(FileContext->Selected, FileContext->Yanked, ArrayCount(FileContext->Yanked));
						FileContext->YankedCount = FileContext->SelectedCount;
						DrawText("Yanked %d selected guys.", FileContext->YankedCount);
						YankeeCount = FileContext->SelectedCount;
					} else if (DirectoryState.Cursor && !IsYanked(FileContext, DirectoryState.Cursor)) { // Otherwise, yank cursor.
						FileContext->Yanked[0] = DirectoryState.Cursor;
						PrintLine("DirectoryState.Cursor : %d, Yanked[0] : %d", DirectoryState.Cursor, FileContext->Yanked[0]);
						DrawText("Yanked %s!", FileContext->Yanked[0]->File.FilePart);
						FileContext->YankedCount = 1;
						YankeeCount = 1;
					}
					
					PushVimCommand(State->VimContext, (vim_command) {
						.Type = VimCommandType_Yank,
						.Yank = {
							.YankeeCount = YankeeCount,
						},
				   });
					
				} else if (ClearYank) {
					if (FileContext->YankedCount) {
						DrawText("Unyanked %s!", FileContext->Yanked[0]->File.FilePart);
						FileContext->YankedCount = 0;
						PushVimCommand(State->VimContext, (vim_command) {
							.Type = VimCommandType_ClearYank,
							.ClearYank = {
								.YankeeCount = FileContext->YankedCount,
							},
					   });
					}
					// TODO(Evan): Invalidate the yankees' directory's cursor.
				} else if (DoPaste) {
					if (FileContext->YankedCount) { // @Cleanup
						u64 PastedCount = 0;
						plore_file_listing *Current = DirectoryState.Current;
						u64 WeAreTopLevel = Platform->IsPathTopLevel(Current->File.AbsolutePath, PLORE_MAX_PATH);
						for (u64 M = 0; M < FileContext->YankedCount; M++) {
							plore_file_listing *Movee = FileContext->Yanked[M];
							
							char NewPath[PLORE_MAX_PATH] = {0};
							u64 BytesWritten = CStringCopy(Current->File.AbsolutePath, NewPath, PLORE_MAX_PATH);
							Assert(BytesWritten < PLORE_MAX_PATH - 1);
							if (!WeAreTopLevel) {
								NewPath[BytesWritten++] = '\\';
							}
							CStringCopy(Movee->File.FilePart, NewPath + BytesWritten, PLORE_MAX_PATH);
							
							b64 DidMoveOk = Platform->MoveFile(Movee->File.AbsolutePath, NewPath);
							if (DidMoveOk) {
								char MoveeCopy[PLORE_MAX_PATH] = {0};
								CStringCopy(Movee->File.AbsolutePath, MoveeCopy, PLORE_MAX_PATH);
								
								RemoveListing(FileContext, Movee);
								
								if (!Platform->IsPathTopLevel(Movee->File.AbsolutePath, PLORE_MAX_PATH)) {
									Platform->PopPathNode(MoveeCopy, PLORE_MAX_PATH, false);
									plore_file_listing *Parent = GetListing(FileContext, MoveeCopy);
									if (Parent) {
										RemoveListing(FileContext, Parent);
									}
								}
								
								PastedCount++;
							} else {
								DrawText("Couldn't paste `%s`", Movee->File.FilePart);
							}
						}
						DrawText("Pasted %d items!", PastedCount);
						
						if (DirectoryState.Current) RemoveListing(FileContext, DirectoryState.Current);
						
						PushVimCommand(State->VimContext, (vim_command) {
							.Type = VimCommandType_Paste,
							.Paste = {
								.PasteeCount = PastedCount,
							},
						});
						
						DirectoryState = SynchronizeCurrentDirectory(State);
						FileContext->YankedCount = 0;
						FileContext->SelectedCount = 0;
						
					}
				} else if (DoMoveLeft) {
					char Buffer[PLORE_MAX_PATH] = {0};
					Platform->GetCurrentDirectory(Buffer, PLORE_MAX_PATH);
					
					Print("Moving up a directory, from %s ...", Buffer);
					Platform->PopPathNode(Buffer, PLORE_MAX_PATH, false);
					PrintLine("to %s ...", Buffer);
					
					Platform->SetCurrentDirectory(Buffer);
					
					PushVimCommand(State->VimContext, (vim_command) {
						.Type = VimCommandType_Movement,
						.Movement = {
							.Direction = Left,
						},
				   });
				} else if (DoMoveRight) {
					plore_file *CursorEntry = FileContext->Current->Entries + FileContext->Current->Cursor;
					
					if (CursorEntry->Type == PloreFileNode_Directory) {
						PrintLine("Moving down a directory, from %s to %s", FileContext->Current->File.AbsolutePath, CursorEntry->AbsolutePath);
						Platform->SetCurrentDirectory(CursorEntry->AbsolutePath);
						
						PushVimCommand(State->VimContext, (vim_command) {
							.Type = VimCommandType_Movement,
							.Movement = {
								.Direction = Right,
							},
					   });
					}
					
				} else if (DoMoveUp && FileContext->Current->Count) {
					if (FileContext->Current->Cursor == 0) {
						FileContext->Current->Cursor = FileContext->Current->Count-1;
					} else {
						FileContext->Current->Cursor -= 1;
					}
					
					PushVimCommand(State->VimContext, (vim_command) {
						.Type = VimCommandType_Movement,
						.Movement = {
							.Direction = Up,
						},
				   });
				} else if (DoMoveDown && FileContext->Current->Count) {
					FileContext->Current->Cursor = (FileContext->Current->Cursor + 1) % FileContext->Current->Count;
					PushVimCommand(State->VimContext, (vim_command) {
						.Type = VimCommandType_Movement,
						.Movement = {
							.Direction = Down,
						},
				   });
				} else if (DoSelect) {
					if (DirectoryState.Cursor) {
						ToggleSelected(FileContext, DirectoryState.Cursor);
						
						PushVimCommand(State->VimContext, (vim_command) {
							.Type = VimCommandType_Select,
							.Select = {
								.SelectCount = 1,
							},
					   });
					}
				}
			
				AteCommands = true;
			} break;
			case InteractState_CommandHistory: {
				PrintLine("Command history mode.");
			} break;
			
			default: {
			} break;
		}
	} else if (CommandCount == MaxCommandCount) {
		AteCommands = true;
	}
	
	DirectoryState = SynchronizeCurrentDirectory(State);
	
	
	//
	// NOTE(Evan): GUI stuff.
	//
	u64 Cols = 3;
	
	f32 FooterHeight = 50;
	f32 fCols = (f32) Cols;
	f32 PadX = 10.0f;
	f32 PadY = 10.0f;
	f32 W = ((PlatformAPI->WindowDimensions.X  - 0)            - (fCols + 1) * PadX) / fCols;
	f32 H = ((PlatformAPI->WindowDimensions.Y - FooterHeight)  - (1     + 1) * PadY) / 1;
	
	f32 X = 0;
	f32 Y = 0;
	
	
	typedef struct plore_viewable_directory {
		plore_file_listing *File;
		b64 Focus;
	} plore_viewable_directory;
	
	plore_viewable_directory ViewDirectories[3] = {
		[0] = {
			.File = FileContext->InTopLevelDirectory ? 0 : DirectoryState.Parent,
		},
		[1] = {
			.File = FileContext->Current,
			.Focus = true,
		},
		[2] = {
			.File = DirectoryState.Cursor,
		},
	};
	
	local char LastCommand[32] = {0};
	if (DidInput) {
		MemoryCopy(Commands, LastCommand, ArrayCount(LastCommand));
	}
	
	if (Window(State->VimguiContext, (vimgui_window_desc) {
					   .Title = FileContext->Current->File.AbsolutePath,
					   .Rect = { .P = V2(0, 0), .Span = { PlatformAPI->WindowDimensions.X, PlatformAPI->WindowDimensions.Y - FooterHeight } },
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
						       .ForceFocus = Directory->Focus })) { // TODO(Evan): Does this do anything?
				
				for (u64 Row = 0; Row < Listing->Count; Row++) {
					v4 Colour = {0};
					plore_file_listing *RowEntry = GetListing(FileContext, Listing->Entries[Row].AbsolutePath);
					if (Listing->Cursor == Row) {
						Colour = V4(0.5, 0.3, 0.3, 0.35);
					} else if (RowEntry) {
						if (IsYanked(FileContext, RowEntry)) {
							Colour = V4(0.3, 0.3, 0.4, 0.45);
						} else if (IsSelected(FileContext, RowEntry)) {
							Colour = V4(0.35, 0.4, 0.4, 0.20);
						}
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
		
		b64 Hidden = State->InteractState != InteractState_CommandHistory;
		if (Window(State->VimguiContext, (vimgui_window_desc) {
						   .Title      = "Command History",
						   .Rect       = { DivideVec2f(PlatformAPI->WindowDimensions, 2), V2(400, 400) }, 
						   .Colour     = V4(1, 1, 1, 0.1),
						   .Hidden = Hidden,
						   .ForceFocus = !Hidden})) {
			for (u64 C = 0; C < State->VimContext->VimCommandCount; C++) {
				if (C == 10) break;
				vim_command *Command = State->VimContext->VimCommands + C;
				char *Title = 0;
				switch (Command->Type) {
					case VimCommandType_Movement: {
						switch (Command->Movement.Direction) {
							case Left: {
								Title = "Movement: Left";
							} break;
							case Right: {
								Title = "Movement: Right";
							} break;
							case Up: {
								Title = "Movement: Up";
							} break;
							case Down: {
								Title = "Movement: Down";
							} break;
						}
					} break;
					case VimCommandType_Select: {
						Title = "Select";
					} break;
					case VimCommandType_Yank: {
						Title = "Yank";
					} break;
					case VimCommandType_Paste: {
						Title = "Paste";
					} break;
					case VimCommandType_ClearYank: {
						Title = "Clear Yank";
					} break;
					default: {
						Title = "I don't feel so good bros...";
					}; break;
					
				}
				if (Button(State->VimguiContext, (vimgui_button_desc) {
								   .Title = Title,
								   .ID = (u64) Command,
								   .FillWidth = true,
								   .Centre = true,
							   })) {
				}
			}
			
			WindowEnd(State->VimguiContext);
		}
		
		if (DirectoryState.Cursor) {
			char CursorInfo[512] = {0};
			StringPrintSized(CursorInfo, 
							 ArrayCount(CursorInfo),
						     "[%s] %s 01-02-3 %s", 
						     (DirectoryState.Cursor->File.Type == PloreFileNode_Directory) ? "DIR" : "FILE", 
						     DirectoryState.Cursor->File.FilePart,
							 LastCommand);
			
			if (Button(State->VimguiContext, (vimgui_button_desc) {
						   .Title = CursorInfo,
						   .Rect = { 
							   .P = V2(0, PlatformAPI->WindowDimensions.X + FooterHeight), .Span = V2(PlatformAPI->WindowDimensions.X, FooterHeight + PadY)
						   },
					   })) {
			}
			
		}
		WindowEnd(State->VimguiContext);
	}
	
	if (AteCommands) {
		CommandCount = 0;
		MemoryClear(Commands, ArrayCount(Commands));
		AteCommands = false;
	}
	
#if defined(PLORE_INTERNAL)
	FlushText();
#endif
	
	VimguiEnd(State->VimguiContext);
	
	
	
	// NOTE(Evan): Right now, we copy this out. We may not want to in the future(tm), even if it is okay now.
	return(*State->VimguiContext->RenderList);
}

plore_inline b64
IsYanked(plore_file_context *Context, plore_file_listing *Yankee) {
	for (u64 Y = 0; Y < Context->YankedCount; Y++) {
		plore_file_listing *It = Context->Yanked[Y];
		if (It == Yankee) return(true);
	}
	
	return(false);
}

plore_inline b64
IsSelected(plore_file_context *Context, plore_file_listing *Selectee) {
	for (u64 S = 0; S < Context->SelectedCount; S++) {
		plore_file_listing *It = Context->Selected[S];
		if (It == Selectee) return(true);
	}
	
	return(false);
}

internal void
ToggleSelected(plore_file_context *Context, plore_file_listing *Selectee) {
	// NOTE(Evan): Toggle and return if its in the list.
	for (u64 S = 0; S < Context->SelectedCount; S++) {
		plore_file_listing *It = Context->Selected[S];
		if (It == Selectee) {
			Context->Selected[S] = Context->Selected[--Context->SelectedCount];
			return;
		}
	}
	
	if (Context->SelectedCount == ArrayCount(Context->Selected) - 1) return;
	
	// NOTE(Evan): Otherwise, add it to the list.
	Context->Selected[Context->SelectedCount++] = Selectee;
}


// TODO(Evan): Invalidate and update FileContext.
internal plore_current_directory_state
SynchronizeCurrentDirectory(plore_state *State) {
	plore_current_directory_state CurrentState = {0};
	
	plore_file_context *FileContext = State->FileContext;
	char Buffer[PLORE_MAX_PATH];
	plore_get_current_directory_result Result = Platform->GetCurrentDirectory(Buffer, PLORE_MAX_PATH);
	FileContext->Current = GetOrInsertListing(FileContext, ListingFromDirectoryPath(Result.AbsolutePath, Result.FilePart)).Listing;
	FileContext->InTopLevelDirectory = Platform->IsPathTopLevel(Buffer, PLORE_MAX_PATH);
	CurrentState.Current = FileContext->Current;
	
	CurrentState.Cursor = 0;
	if (FileContext->Current->Count != 0) {
		plore_file *CursorEntry = FileContext->Current->Entries + FileContext->Current->Cursor;
		
		CurrentState.Cursor = GetOrInsertListing(FileContext, ListingFromFile(CursorEntry)).Listing;
	}
	
	if (!FileContext->InTopLevelDirectory) { 
		// CLEANUP(Evan): Doesn't need to copy twice!
		plore_file_listing CurrentCopy = *FileContext->Current;
		plore_pop_path_node_result Result = Platform->PopPathNode(CurrentCopy.File.AbsolutePath, ArrayCount(CurrentCopy.File.AbsolutePath), false);
		
		plore_file_listing_get_or_insert_result ParentResult = GetOrInsertListing(FileContext, ListingFromDirectoryPath(Result.AbsolutePath, Result.FilePart));
		
		// Make sure the parent's cursor is set to the current directory.
		// The parent's row will be invalid on plore startup.
		CurrentState.Parent = ParentResult.Listing;
		if (!ParentResult.DidAlreadyExist) {
			for (u64 Row = 0; Row < CurrentState.Parent->Count; Row++) {
				plore_file *File = CurrentState.Parent->Entries + Row;
				if (File->Type != PloreFileNode_Directory) continue;
				
				if (CStringsAreEqual(File->AbsolutePath, FileContext->Current->File.AbsolutePath)) {
					CurrentState.Parent->Cursor = Row;
					break;
				}
			}
			
		}
		
	}
	
	
	return(CurrentState);
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
