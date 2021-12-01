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

platform_texture_handle TextureHandle_CantLoad;
memory_arena *STBFrameArena = 0;
internal void *
STBMalloc(u64 Size) {
	void *Result = PushBytes(STBFrameArena, Size);
	Assert(Result);
	
	return(Result);
}

internal void
STBFree(void *Ptr) {
	(void)Ptr;
}

internal void *
STBRealloc(void *Ptr, u64 Size) {
	(void)Ptr;
	
	void *Result = STBMalloc(Size);
	Assert(Result);
	
	return(Result);
}

#define STBI_MALLOC(Size) STBMalloc(Size)
#define STBI_FREE(Ptr) STBFree(Ptr)
#define STBI_REALLOC(Ptr, Size) STBRealloc(Ptr, Size)
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef struct plore_current_directory_state {
	union {
		struct {
			plore_file_listing Current;
			plore_file_listing Parent;
			plore_file_listing Cursor;
		};
		plore_file_listing Listings[3];
	};
} plore_current_directory_state;

typedef struct plore_state {
	b64 Initialized;
	f64 DT;
	plore_current_directory_state DirectoryState;
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


typedef struct load_image_result {
	platform_texture_handle Texture;
	b64 LoadedSuccessfully;
	char *ErrorReason;
} load_image_result;

internal load_image_result
LoadImage(memory_arena *Arena, char *AbsolutePath, u64 MaxX, u64 MaxY);

internal void
PrintDirectory(plore_file_listing_cursor *Directory);

plore_inline b64
IsYanked(plore_file_context *Context, plore_path *Yankee);

plore_inline b64
IsSelected(plore_file_context *Context, plore_path *Selectee);

internal void
ToggleSelected(plore_file_context *Context, plore_path *Selectee);

internal char *
PloreKeysToString(memory_arena *Arena, vim_key *Keys, u64 KeyCount);

internal void
SynchronizeCurrentDirectory(plore_file_context *FileContext, plore_current_directory_state *CurrentState);

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
		Result->Fonts[F].Handle = Platform->CreateTextureHandle((platform_texture_handle_desc) {
																	.Pixels = TempBitmap, 
																	.Height = 512, 
																	.Width = 512,
																	.TargetPixelFormat = PixelFormat_ALPHA,
																	.ProvidedPixelFormat = PixelFormat_ALPHA,
																	.FilterMode = FilterMode_Linear
																});
		
	}
	Platform->DebugCloseFile(FontFile);
	
	return(Result);
}

PLORE_DO_ONE_FRAME(PloreDoOneFrame) {
	plore_state *State = (plore_state *)PloreMemory->PermanentStorage.Memory;
	
	if (!State->Initialized) {
		PlatformInit(PlatformAPI);
		State = PushStruct(&PloreMemory->PermanentStorage, plore_state);
		
		State->Initialized = true;
		State->Memory = PloreMemory;
		State->FrameArena = SubArena(&State->Memory->PermanentStorage, Megabytes(128), 16);
		STBFrameArena = &State->FrameArena;
		
		// MAINTENANCE(Evan): Don't copy the main arena by value until we've allocated the frame arena.
		State->Arena = PloreMemory->PermanentStorage;
		
		State->FileContext = PushStruct(&State->Arena, plore_file_context);
		for (u64 Dir = 0; Dir < ArrayCount(State->FileContext->CursorSlots); Dir++) {
			State->FileContext->CursorSlots[Dir] = PushStruct(&State->Arena, plore_file_listing_cursor);
		}
		
		State->Font = FontInit(&State->Arena, "data/fonts/Inconsolata-Light.ttf");
		
#if defined(PLORE_INTERNAL)
		DebugInit(State);
#endif
		
		State->VimContext = PushStruct(&State->Arena, plore_vim_context);
		State->VimContext->MaxCommandCount = 2; // @Hack
		
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
	keyboard_and_mouse BufferedInput = PloreInput->ThisFrame;
	
	local f32 LastTime;
	State->DT = PloreInput->Time - LastTime;
	LastTime = PloreInput->Time;
	
	// CLEANUP(Evan): We Begin() here as the render list is shared for debug draw purposes right now.
	VimguiBegin(State->VimguiContext, BufferedInput, PlatformAPI->WindowDimensions);
	
	// TODO(Evan): File watching so this function doesn't need to be called eagerly.
	SynchronizeCurrentDirectory(State->FileContext, &State->DirectoryState);
	
	if (BufferedInput.OIsPressed) {
		if (State->InteractState == InteractState_CommandHistory) {
			State->InteractState = InteractState_FileExplorer;
		} else {
			State->InteractState = InteractState_CommandHistory;
		}
	}
	
	// NOTE(Evan): Buffered input.
#define PLORE_X(Key, _Ignored1, _Ignored2) local u64 Key##Count = 0;
	PLORE_KEYBOARD_AND_MOUSE
#undef PLORE_X
	
#define PLORE_X(Key, _Ignored1, _Ignored2) \
	if (BufferedInput.##Key##IsDown) Key##Count++;                    \
	else Key##Count = 0;                                              \
	if (Key##Count > 20) { Key##Count = 10; }                         \
	else if (Key##Count == 20) {                                      \
		BufferedInput.##Key##IsPressed = true;                        \
		BufferedInput.pKeys[PloreKey_##Key##] = true;                 \
		BufferedInput.bKeys[PloreKey_##Key##] = true;                 \
	}
	
	PLORE_KEYBOARD_AND_MOUSE
#undef PLORE_X
	
	if (BufferedInput.sKeys[PloreKey_Space]) {
		PrintLine("SHIFT + SPACE!");
	}
	b64 DidInput = false;
	if (State->VimContext->CommandKeyCount < ArrayCount(State->VimContext->CommandKeys)) {
		for (plore_key K = 0; K < ArrayCount(BufferedInput.pKeys); K++) {
			if (BufferedInput.pKeys[K] && (K != PloreKey_Shift && K != PloreKey_Ctrl)) {
				vim_key TheKey = { .Input = K };
				PrintLine("Key %s was given to vim command keys.", PloreKeyStrings[K]);
				if (BufferedInput.sKeys[K]) {
					PrintLine("shift modifier");
					TheKey.Modifier = PloreKey_Shift;
				}
				
				State->VimContext->CommandKeys[State->VimContext->CommandKeyCount++] = TheKey;
				DidInput = true;
				break;
			}
		}
	}
	
#if 0
	local plore_key LastCommand[32] = {0};
	local u64 LastCommandCount = 0;
	if (DidInput) {
		PrintLine("DID INPUT.");
		MemoryCopy(State->VimContext->CommandKeys, LastCommand, sizeof(LastCommand));
		LastCommandCount = State->VimContext->CommandKeyCount;
	}
#endif
	
	
	local b64 AteCommands = 0;
	//
	// NOTE(Evan): Vim command processing
	//
	if (DidInput) {
		vim_command Command = MakeCommand(State->VimContext);
		if (Command.Type != VimCommandType_None) {
			switch (State->InteractState) {
				case (InteractState_FileExplorer): {
					Command.State = InteractState_FileExplorer;
					
					switch (Command.Type) {
						case VimCommandType_Yank: {
							if (FileContext->SelectedCount) { // Yank selection if there is any.
								MemoryCopy(FileContext->Selected, FileContext->Yanked, sizeof(FileContext->Yanked));
								FileContext->YankedCount = FileContext->SelectedCount;
								DrawText("Yanked %d guys.", FileContext->YankedCount);
									
								PushVimCommand(State->VimContext, (vim_command) {
												   .State = InteractState_FileExplorer,
												   .Type = VimCommandType_Yank,
											   });
							} 
						
						} break;
						case VimCommandType_ClearYank: {
							if (FileContext->YankedCount) {
								DrawText("Unyanked %d guys", FileContext->YankedCount);
								FileContext->YankedCount = 0;
								FileContext->SelectedCount = 0;
								PushVimCommand(State->VimContext, (vim_command) {
												   .State = InteractState_FileExplorer,
												   .Type = VimCommandType_ClearYank,
											   });
							}
							// TODO(Evan): Invalidate the yankees' directory's cursor.
						} break;
						case VimCommandType_Paste: {
							if (FileContext->YankedCount) { // @Cleanup
								u64 PastedCount = 0;
								plore_file_listing *Current = &State->DirectoryState.Current;
								u64 WeAreTopLevel = Platform->IsPathTopLevel(Current->File.Path.Absolute, PLORE_MAX_PATH);
								for (u64 M = 0; M < FileContext->YankedCount; M++) {
									plore_path *Movee = FileContext->Yanked + M;
									
									char NewPath[PLORE_MAX_PATH] = {0};
									u64 BytesWritten = CStringCopy(Current->File.Path.Absolute, NewPath, PLORE_MAX_PATH);
									Assert(BytesWritten < PLORE_MAX_PATH - 1);
									if (!WeAreTopLevel) {
										NewPath[BytesWritten++] = '\\';
									}
									CStringCopy(Movee->FilePart, NewPath + BytesWritten, PLORE_MAX_PATH);
									
									b64 DidMoveOk = Platform->MoveFile(Movee->Absolute, NewPath);
									if (DidMoveOk) {
										PastedCount++;
									} else {
										DrawText("Couldn't paste `%s`", Movee->FilePart);
									}
								}
								DrawText("Pasted %d items!", PastedCount);
								
								PushVimCommand(State->VimContext, (vim_command) {
												   .State = InteractState_FileExplorer,
												   .Type = VimCommandType_Paste,
											   });
								
								FileContext->YankedCount = 0;
								MemoryClear(FileContext->Yanked, sizeof(FileContext->Yanked));
								
								FileContext->SelectedCount = 0;
								MemoryClear(FileContext->Selected, sizeof(FileContext->Selected));
							}
						} break;
						case VimCommandType_MoveLeft: {
							u64 ScalarCount = 0;
							while (Command.Scalar--) {
								char Buffer[PLORE_MAX_PATH] = {0};
								Platform->GetCurrentDirectory(Buffer, PLORE_MAX_PATH);
								
								Print("Moving up a directory, from %s ", Buffer);
								Platform->PopPathNode(Buffer, PLORE_MAX_PATH, false);
								PrintLine("to %s", Buffer);
								
								Platform->SetCurrentDirectory(Buffer);
								ScalarCount++;
								
								if (Platform->IsPathTopLevel(Buffer, PLORE_MAX_PATH)) break;
							}
							Command.Scalar = ScalarCount;
							PushVimCommand(State->VimContext, Command);
						} break;
						
						case VimCommandType_MoveRight: {
							u64 WasGivenScalar = Command.Scalar > 1;
							u64 ScalarCount = 0;
							while (Command.Scalar--) {
								plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(State->FileContext, &State->DirectoryState.Current.File.Path);
								plore_file *CursorEntry = State->DirectoryState.Current.Entries + CursorResult.Cursor->Cursor;
							
								if (CursorEntry->Type == PloreFileNode_Directory) {
									PrintLine("Moving down a directory, from %s to %s", 
											  State->DirectoryState.Current.File.Path.Absolute, 
											  CursorEntry->Path.Absolute);
									Platform->SetCurrentDirectory(CursorEntry->Path.Absolute);
									
									ScalarCount++;
									
									if (Platform->IsPathTopLevel(CursorEntry->Path.Absolute, PLORE_MAX_PATH)) break;
									else SynchronizeCurrentDirectory(State->FileContext, &State->DirectoryState);
								} else if (!WasGivenScalar) {
									if (CursorEntry->Extension != PloreFileExtension_Unknown) {
										DrawText("Opening %s", CursorEntry->Path.FilePart);
										Platform->RunShell(PloreFileExtensionHandlers[CursorEntry->Extension], CursorEntry->Path.Absolute);
									} else {
										DrawText("Unknown extension - specify a handler", CursorEntry->Path.FilePart);
									}
								}
								
								if (!State->DirectoryState.Current.Count) break;
								
							}
							Command.Scalar = ScalarCount;
							PushVimCommand(State->VimContext, Command);
						} break;
						
						case VimCommandType_MoveUp: {
							if (State->DirectoryState.Current.Count) {
								plore_file_listing_cursor_get_or_create_result CurrentResult = GetOrCreateCursor(State->FileContext, &State->DirectoryState.Current.File.Path);
								u64 Magnitude = Command.Scalar % State->DirectoryState.Current.Count;
								if (CurrentResult.Cursor->Cursor == 0) {
									CurrentResult.Cursor->Cursor = State->DirectoryState.Current.Count-Magnitude;
								} else {
									CurrentResult.Cursor->Cursor -= Magnitude;
								}
								
								PushVimCommand(State->VimContext, Command);
							}
						} break;
						
						case VimCommandType_MoveDown: {
							if (State->DirectoryState.Current.Count) {
								plore_file_listing_cursor_get_or_create_result CurrentResult = GetOrCreateCursor(State->FileContext, &State->DirectoryState.Current.File.Path);
								u64 Magnitude = Command.Scalar % State->DirectoryState.Current.Count;
								
								CurrentResult.Cursor->Cursor = (CurrentResult.Cursor->Cursor + Magnitude) % State->DirectoryState.Current.Count;
								PushVimCommand(State->VimContext, Command);
							}
						} break;
						
						case VimCommandType_SelectDown:
						case VimCommandType_SelectUp: {
							i64 Direction = Command.Type == VimCommandType_SelectDown ? 1 : -1;
							plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(State->FileContext, &State->DirectoryState.Current.File.Path);
							plore_file *Selectee = State->DirectoryState.Current.Entries + CursorResult.Cursor->Cursor;
							u64 ScalarCount = 0;
							u64 StartScalar = CursorResult.Cursor->Cursor;
							while (Command.Scalar--) {
								ToggleSelected(FileContext, &State->DirectoryState.Cursor.File.Path);
								if (Direction == +1) {
									CursorResult.Cursor->Cursor = (CursorResult.Cursor->Cursor + 1) % State->DirectoryState.Current.Count;
								} else {
									if (CursorResult.Cursor->Cursor == 0) {
										CursorResult.Cursor->Cursor = State->DirectoryState.Current.Count - 1;
									} else {
										CursorResult.Cursor->Cursor -= 1;
									}
								}
								Selectee = State->DirectoryState.Current.Entries + CursorResult.Cursor->Cursor;
								
								ScalarCount++;
								if (CursorResult.Cursor->Cursor == StartScalar) break;
							}
							
							PushVimCommand(State->VimContext, Command);
						} break;
						
						case VimCommandType_Incomplete: {
							PrintLine("INCOMPLETE");
						} break;
						
						InvalidDefaultCase;
						
					}
					if (Command.Type != VimCommandType_Incomplete) {
						PrintLine("Command is not incomplete, clearing command list.");
						AteCommands = true;
					}
					
			} break;
			case InteractState_CommandHistory: {
				PrintLine("Command history mode.");
			} break;
				
			}
		} else if (State->VimContext->CommandKeyCount == State->VimContext->MaxCommandCount) {
			AteCommands = true;
		}
	}
	SynchronizeCurrentDirectory(State->FileContext, &State->DirectoryState);
	
	
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
	f32 X = 10;
	f32 Y = 0;
	
	
	
	typedef struct plore_viewable_directory {
		plore_file_listing *File;
		b64 Focus;
	} plore_viewable_directory;
	
	plore_viewable_directory ViewDirectories[3] = {
		[0] = {
			.File = FileContext->InTopLevelDirectory ? 0 : &State->DirectoryState.Parent,
		},
		[1] = {
			.File = &State->DirectoryState.Current,
			.Focus = true,
		},
		[2] = {
			.File = &State->DirectoryState.Cursor.Valid ? &State->DirectoryState.Cursor : 0,
		},
	};
	
	typedef struct image_preview_handle {
		plore_path Path;
		platform_texture_handle Texture;
		b64 Allocated;
	} image_preview_handle;
	
	if (Window(State->VimguiContext, (vimgui_window_desc) {
				   .Title = State->DirectoryState.Current.File.Path.Absolute,
				   .Rect = { .P = V2(0, 0), .Span = { PlatformAPI->WindowDimensions.X, PlatformAPI->WindowDimensions.Y - FooterHeight } },
			   })) {
		
		
#if 1
		if (State->DirectoryState.Cursor.Valid) {
			char CursorInfo[512] = {0};
			StringPrintSized(CursorInfo, 
							 ArrayCount(CursorInfo),
						     "[%s] %s 01-02-3 %s", 
						     (State->DirectoryState.Cursor.File.Type == PloreFileNode_Directory) ? "DIR" : "FILE", 
						     State->DirectoryState.Cursor.File.Path.FilePart,
							 PloreKeysToString(&State->FrameArena, State->VimContext->CommandKeys, State->VimContext->CommandKeyCount));
			
			if (Button(State->VimguiContext, (vimgui_button_desc) {
						   .Title = CursorInfo,
						   .Rect = { 
							   .P = V2(0, PlatformAPI->WindowDimensions.X + FooterHeight), 
							   .Span = V2(PlatformAPI->WindowDimensions.X, FooterHeight + PadY)
						   },
					   })) {
			}
			
		}
#endif
		for (u64 Col = 0; Col < Cols; Col++) {
			v2 P      = V2(X, 0);
			v2 Span   = V2(W-3, H-22);
			
			plore_viewable_directory *Directory = ViewDirectories + Col;
			plore_file_listing *Listing = Directory->File;
			if (!Listing) continue; /* Parent can be null, if we are currently looking at a top-level directory. */
			
			
			plore_file_listing_cursor *RowCursor = GetCursor(State->FileContext, Listing->File.Path.Absolute);
			char *Title = Listing->File.Path.FilePart;
			if (Window(State->VimguiContext, (vimgui_window_desc) {
							   .Title                = Title,
							   .Rect                 = {P, Span}, 
							   .ForceFocus           = Directory->Focus,
							   .BackgroundColour     = WidgetColour_Primary,
						   })) {
				
				switch (Listing->File.Type) {
					case PloreFileNode_Directory: {
						for (u64 Row = 0; Row < Listing->Count; Row++) {
							widget_colour BackgroundColour = 0;
							text_colour TextColour = 0;
							
							plore_file *RowEntry = Listing->Entries + Row;
							
							if (IsYanked(FileContext, &RowEntry->Path)) {
								BackgroundColour = WidgetColour_Tertiary;
							} else if (IsSelected(FileContext, &RowEntry->Path)) {
								BackgroundColour = WidgetColour_Quaternary;
							} else if (RowCursor && RowCursor->Cursor == Row) {
								BackgroundColour = WidgetColour_Secondary;
							}
							
							if (RowEntry->Type == PloreFileNode_Directory) {
								TextColour = TextColour_Primary;
							} else {
								TextColour = TextColour_Secondary;
							}
							
							if (Button(State->VimguiContext, (vimgui_button_desc) {
										   .Title = Listing->Entries[Row].Path.FilePart,
										   .FillWidth = true,
										   .Centre = true,
										   .BackgroundColour = BackgroundColour,
										   .TextColour = TextColour,
										   .TextPad = V2(4, 0),
										   })) {
								// TODO(Evan): Change cursor to here?!
								PrintLine("Button %s was clicked!", Listing->Entries[Row].Path.FilePart);
								if (Listing->Entries[Row].Type == PloreFileNode_Directory) {
									PrintLine("Changing Directory to %s", Listing->Entries[Row].Path.Absolute);
									Platform->SetCurrentDirectory(Listing->Entries[Row].Path.Absolute);
								}
							}
						}
					} break;
					case PloreFileNode_File: {
						
						local image_preview_handle ImagePreviewHandles[32] = {0};
						local u64 ImagePreviewHandleCursor = 0;
						
						image_preview_handle *MyHandle = 0;
						
						// TODO(Evan): Make this work for other image types
						// TODO(Evan): Make file loading asynchronous!
						switch (Listing->File.Extension) {
							case PloreFileExtension_BMP:
							case PloreFileExtension_PNG:
							case PloreFileExtension_JPG: {
								for (u64 I = 0; I < ArrayCount(ImagePreviewHandles); I++) {
									image_preview_handle *Handle = ImagePreviewHandles + I;
									if (CStringsAreEqual(Handle->Path.Absolute, Listing->File.Path.Absolute)) {
										MyHandle = Handle;
										break;
									}
								}
								if (!MyHandle) {
									MyHandle = ImagePreviewHandles + ImagePreviewHandleCursor;
									ImagePreviewHandleCursor = (ImagePreviewHandleCursor + 1) % ArrayCount(ImagePreviewHandles);
									
									if (MyHandle->Allocated) {
										Platform->DestroyTextureHandle(MyHandle->Texture);
										MyHandle->Texture = (platform_texture_handle) {0};
									}
									MyHandle->Path = Listing->File.Path;
									
									load_image_result ImageResult = LoadImage(&State->FrameArena, Listing->File.Path.Absolute, 1024, 1024);
									
									if (!ImageResult.LoadedSuccessfully) {
//										DrawText("%s", ImageResult.ErrorReason);
										MyHandle->Texture = TextureHandle_CantLoad;
									} else {
										MyHandle->Texture = ImageResult.Texture;
									}
									
								}
							} break;
						}
						
						if (MyHandle) { 
							if (Image(State->VimguiContext, (vimgui_image_desc) { 
											  .ID = (u64) MyHandle,
											  .Texture = MyHandle->Texture,
											  .Rect = {
												  .Span = V2(512, 512),
											  }, 
											  .Centered = true,
										  })) {
							}
						}
					} break;
					
					InvalidDefaultCase;
				}
				WindowEnd(State->VimguiContext);
			}
			
			X += W + PadX;
		}
		
		b64 Hidden = State->InteractState != InteractState_CommandHistory;
		if (Window(State->VimguiContext, (vimgui_window_desc) {
					   .Title              = "Command History",
					   .Rect               = { DivideVec2f(PlatformAPI->WindowDimensions, 2), V2(400, 400) }, 
					   .Hidden             = Hidden,
					   .ForceFocus         = !Hidden})) {
			for (vim_command_type C = 0; C < State->VimContext->VimCommandCount; C++) {
				if (C == 10) break;
				vim_command *Command = State->VimContext->VimCommands + C;
				char *Title = VimCommandStrings[Command->Type];
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
		
		WindowEnd(State->VimguiContext);
	}
	
	if (AteCommands) {
		PrintLine("Ate commands, clearing vim command keys");
		AteCommands = false;
		
		MemoryClear(State->VimContext->CommandKeys, sizeof(State->VimContext->CommandKeys));
		State->VimContext->CommandKeyCount = 0;
#if 0
		LastCommandCount = 0;
		MemoryClear(LastCommand, sizeof(LastCommand));
#endif
	}
	
#if defined(PLORE_INTERNAL)
	FlushText();
#endif
	
	
	VimguiEnd(State->VimguiContext);
	
	
	for (u64 S = 0; S < State->FileContext->SelectedCount; S++) {
		PrintLine("Selected %d is %s", S, State->FileContext->Selected[S]);
	}
	
	for (u64 S = 0; S < State->FileContext->YankedCount; S++) {
		PrintLine("Yanked %d is %s", S, State->FileContext->Yanked[S]);
	}
	
	// NOTE(Evan): Right now, we copy this out. We may not want to in the future(tm), even if it is okay now.
	return(*State->VimguiContext->RenderList);
}

plore_inline b64
IsYanked(plore_file_context *Context, plore_path *Yankee) {
	for (u64 Y = 0; Y < Context->YankedCount; Y++) {
		plore_path *It = Context->Yanked + Y;
		if (CStringsAreEqual(It->Absolute, Yankee->Absolute)) return(true);
	}
	
	return(false);
}

plore_inline b64
IsSelected(plore_file_context *Context, plore_path *Selectee) {
	for (u64 S = 0; S < Context->SelectedCount; S++) {
		plore_path *It = Context->Selected + S;
		if (CStringsAreEqual(It->Absolute, Selectee->Absolute)) return(true);
	}
	
	return(false);
}

internal void
ToggleSelected(plore_file_context *Context, plore_path *Selectee) {
	Assert(Selectee);
	
	// NOTE(Evan): Toggle and return if its in the list.
	for (u64 S = 0; S < Context->SelectedCount; S++) {
		plore_path *It = Context->Selected + S;
		if (CStringsAreEqual(It->Absolute, Selectee->Absolute)) {
			Context->Selected[S] = Context->Selected[--Context->SelectedCount];
			return;
		}
	}
	
	if (Context->SelectedCount == ArrayCount(Context->Selected) - 1) return;
	
	// NOTE(Evan): Otherwise, add it to the list.
	Context->Selected[Context->SelectedCount++] = *Selectee;
	DrawText("There are now %d selected items.", Context->SelectedCount);
}


internal void
CreateFileListing(plore_file_listing *Listing, plore_file_listing_desc Desc) {
	Assert(Listing);
	
	Listing->File.Type = Desc.Type;
	CStringCopy(Desc.Path.Absolute, Listing->File.Path.Absolute, PLORE_MAX_PATH);
	CStringCopy(Desc.Path.FilePart, Listing->File.Path.FilePart, PLORE_MAX_PATH);
	if (Desc.Type == PloreFileNode_Directory) {
		directory_entry_result CurrentDirectory = Platform->GetDirectoryEntries(Listing->File.Path.Absolute, 
																				Listing->Entries, 
																				ArrayCount(Listing->Entries));
		if (!CurrentDirectory.Succeeded) {
			PrintLine("Could not get current directory.");
		} else {
			Listing->Count = CurrentDirectory.Count;
			Listing->Valid = true;
		}
	} else { 
		// NOTE(Evan): GetDirectoryEntries does this for directories!
		Listing->File.Extension = GetFileExtension(Desc.Path.FilePart).Extension; 
		Listing->Valid = true;
	}
	
}
internal void
SynchronizeCurrentDirectory(plore_file_context *FileContext, plore_current_directory_state *CurrentState) {
	plore_path CurrentDir = Platform->GetCurrentDirectoryPath();
	
	CurrentState->Cursor = (plore_file_listing) {0};
	CurrentState->Current = (plore_file_listing) {0};
	
	CreateFileListing(&CurrentState->Current, ListingFromDirectoryPath(&CurrentDir));
	FileContext->InTopLevelDirectory = Platform->IsPathTopLevel(CurrentState->Current.File.Path.Absolute, PLORE_MAX_PATH);
	
	if (CurrentState->Current.Valid && CurrentState->Current.Count) {
		plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(FileContext, &CurrentState->Current.File.Path);
		plore_file *CursorEntry = CurrentState->Current.Entries + CursorResult.Cursor->Cursor;
		CreateFileListing(&CurrentState->Cursor, ListingFromFile(CursorEntry));
	}
	
	if (!FileContext->InTopLevelDirectory) {
		// CLEANUP(Evan): Doesn't need to copy twice!
		plore_file_listing CurrentCopy = CurrentState->Current;
		Platform->PopPathNode(CurrentCopy.File.Path.Absolute, ArrayCount(CurrentCopy.File.Path.Absolute), false);
		
		if (Platform->IsPathTopLevel(CurrentCopy.File.Path.Absolute, PLORE_MAX_PATH)) {
			CStringCopy(CurrentCopy.File.Path.Absolute, CurrentCopy.File.Path.FilePart, PLORE_MAX_PATH);
		}
		CreateFileListing(&CurrentState->Parent, ListingFromDirectoryPath(&CurrentCopy.File.Path));
		
		plore_file_listing_cursor_get_or_create_result ParentResult = GetOrCreateCursor(FileContext, &CurrentState->Parent.File.Path);
		
		// Make sure the parent's cursor is set to the current directory.
		// The parent's row will be invalid on plore startup.
		if (!ParentResult.DidAlreadyExist) {
			for (u64 Row = 0; Row < CurrentState->Parent.Count; Row++) {
				plore_file *File = CurrentState->Parent.Entries + Row;
				if (File->Type != PloreFileNode_Directory) continue;
				
				if (CStringsAreEqual(File->Path.Absolute, CurrentState->Current.File.Path.Absolute)) {
					ParentResult.Cursor->Cursor = Row;
					break;
				}
			}
			
		}
		
		
	} else {
		CurrentState->Parent.Valid = false;
	}
}

internal char *
PloreKeysToString(memory_arena *Arena, vim_key *Keys, u64 KeyCount) {
	u64 Size = KeyCount*8+1;
	u64 Count = 0;
	if (!KeyCount || !Keys) return("");
	
	char *Result = PushBytes(Arena, Size);
	char *Buffer = Result; 
	
	for (u64 K = 0; K < KeyCount; K++) {
		Count += CStringCopy(PloreKeyStrings[Keys[K].Input], Buffer, Size - Count);
		Buffer += Count;
	}
	
	return(Result);
}

internal load_image_result
LoadImage(memory_arena *Arena, char *AbsolutePath, u64 MaxX, u64 MaxY) {
	load_image_result Result = {0};
	i32 X, Y, Comp;
	
	platform_readable_file File = Platform->DebugOpenFile(AbsolutePath);
	Assert(File.OpenedSuccessfully);
	
	u64 Size = MaxX*MaxY*4*4;
	u8 *Buffer = PushBytes(Arena, Size);
	platform_read_file_result FileResult = Platform->DebugReadEntireFile(File, Buffer, Size);
	Assert(FileResult.ReadSuccessfully);
	
	stbi_set_flip_vertically_on_load(true);
	u8 *Pixels = stbi_load_from_memory(Buffer, Size, &X, &Y, &Comp, 0);
	
	Platform->DebugCloseFile(File);
	if (Pixels) {
		Result.LoadedSuccessfully = true;
		Result.Texture = Platform->CreateTextureHandle((platform_texture_handle_desc) {
														   .Pixels = Pixels, 
														   .Width = X, 
														   .Height = Y,
														   .ProvidedPixelFormat = Comp == 4 ? PixelFormat_RGBA8 : PixelFormat_RGB8,
														   .TargetPixelFormat = PixelFormat_RGBA8,
														   .FilterMode = FilterMode_Nearest,
													   });
	} else {
		Result.Texture = TextureHandle_CantLoad;
		Result.ErrorReason = (char *)stbi_failure_reason();
	}
	
	
	return(Result);
}
	