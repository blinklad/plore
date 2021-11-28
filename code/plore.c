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
#define BITMAP_COMPRESSION_NONE 0
#define BITMAP_COMPRESSION_RUN_LENGTH_ENCODING_8 1
#define BITMAP_COMPRESSION_RUN_LENGTH_ENCODING_4 2
#define BITMAP_COMPRESSION_RGB_WITH_MASK 3

#pragma pack(push, 1)
typedef struct bitmap_header {
	u16 Name;
	u32 SizeInBytes;
	u32 Reserved;
	u32 DataOffsetFromHeaderStart;
	u32 HeaderSize;
	i32 Width;
	i32 Height;
	u16 Planes;
	u16 BitsPerPixel;
	u32 CompressionType;
	u32 ImageSizeInBytes;
	i32 XResolutionInMeters;
	i32 YResolutionInMeters;
	u32 ColourCount;
	u32 ImportantColours;
} bitmap_header;
#pragma pack(pop)

typedef struct loaded_bitmap {
	u32 Width;
	u32 Height;
	void *Memory;
} loaded_bitmap;

internal loaded_bitmap
LoadBMP(void *Memory) {
	bitmap_header *BitmapHeader = (bitmap_header *)Memory;
	
	Assert(BitmapHeader);
	Assert(((char *)BitmapHeader)[0] == 'B'); 
	Assert(((char *)BitmapHeader)[1] == 'M'); 
	Assert(BitmapHeader->Height > 0); // NOTE(Evan): Negative height means top-down instead of bottom-up!
	loaded_bitmap Result = {
		.Memory = ((u8 *)BitmapHeader + BitmapHeader->DataOffsetFromHeaderStart),
		.Width = (u32)BitmapHeader->Width,
		.Height = (u32)BitmapHeader->Height,
	};
	
	return(Result);
}


internal void
PrintDirectory(plore_file_listing *Directory);

plore_inline b64
IsYanked(plore_file_context *Context, plore_file_listing *Yankee);

plore_inline b64
IsSelected(plore_file_context *Context, plore_file_listing *Selectee);

internal void
ToggleSelected(plore_file_context *Context, plore_file_listing *Selectee);

internal char *
PloreKeysToString(memory_arena *Arena, plore_key *Keys, u64 KeyCount);

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
		State->FrameArena = SubArena(&State->Memory->PermanentStorage, Megabytes(32), 16);
		
		// MAINTENANCE(Evan): Don't copy the main arena by value until we've allocated the frame arena.
		State->Arena = PloreMemory->PermanentStorage;
		
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
	plore_current_directory_state DirectoryState = SynchronizeCurrentDirectory(State);
	
	if (BufferedInput.OIsPressed) {
		if (State->InteractState == InteractState_CommandHistory) {
			State->InteractState = InteractState_FileExplorer;
		} else {
			State->InteractState = InteractState_CommandHistory;
		}
	}
	
	//ThisFrame->InputChar = SymbolicName + (ThisFrame->ShiftIsDown ? 32 : 0); \
	// NOTE(Evan): Buffered input.
#define PLORE_X(Key, _Ignored) local u64 Key##Count = 0;
	PLORE_KEYBOARD_AND_MOUSE
#undef PLORE_X
	
#define PLORE_X(Key, _Ignored) \
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
	
	
	typedef struct vim_key {
		plore_key Input;
		b64 DisableBufferedMove;
	} vim_key;
	
	vim_key RegisteredKeys[] = { 
		{ PloreKey_Y,                              },
		{ PloreKey_U,                              },
		{ PloreKey_P,                              },
		{ PloreKey_Space,                          },
		{ PloreKey_J,                              },
		{ PloreKey_K,                              },
		{ PloreKey_H,                              },
		{ PloreKey_L,  .DisableBufferedMove = true },
	};
	
	b64 DidInput = false;
	if (State->VimContext->CommandKeyCount < ArrayCount(State->VimContext->CommandKeys)) {
		for (u64 K = 0; K < ArrayCount(RegisteredKeys); K++) {
			vim_key *TheKey = RegisteredKeys + K;
			if (BufferedInput.pKeys[TheKey->Input]) {
				if (BufferedInput.bKeys[TheKey->Input] && TheKey->DisableBufferedMove) continue;
				
				State->VimContext->CommandKeys[State->VimContext->CommandKeyCount++] = TheKey->Input;
				DidInput = true;
				break;
			}
		}
	}
	
	local plore_key LastCommand[32] = {0};
	local u64 LastCommandCount = 0;
	if (DidInput) {
		MemoryCopy(State->VimContext->CommandKeys, LastCommand, sizeof(LastCommand));
		LastCommandCount = State->VimContext->CommandKeyCount;
	}
	vim_command Command = MakeCommand(State->VimContext);
	
	
	local b64 AteCommands = 0;
	//
	// NOTE(Evan): Vim command processing
	//
	if (Command.Type != VimCommandType_None) {
		switch (State->InteractState) {
			case (InteractState_FileExplorer): {
				switch (Command.Type) {
					case VimCommandType_Yank: {
						if (FileContext->SelectedCount) { // Yank selection if there is any.
							MemoryCopy(FileContext->Selected, FileContext->Yanked, sizeof(FileContext->Yanked));
							FileContext->YankedCount = FileContext->SelectedCount;
							DrawText("Yanked %d guys.", FileContext->YankedCount);
								
							PushVimCommand(State->VimContext, (vim_command) {
											   .State = InteractState_FileExplorer,
											   .Type = VimCommandType_Yank,
											   .Yank = {
												   .YankeeCount = FileContext->YankedCount,
											   },
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
											   .ClearYank = {
												   .YankeeCount = FileContext->YankedCount,
											   },
										   });
						}
						// TODO(Evan): Invalidate the yankees' directory's cursor.
					} break;
					case VimCommandType_Paste: {
						if (FileContext->YankedCount) { // @Cleanup
							u64 PastedCount = 0;
							plore_file_listing *Current = DirectoryState.Current;
							u64 WeAreTopLevel = Platform->IsPathTopLevel(Current->File.Path.Absolute, PLORE_MAX_PATH);
							for (u64 M = 0; M < FileContext->YankedCount; M++) {
								plore_file_listing *Movee = FileContext->Yanked[M];
								
								char NewPath[PLORE_MAX_PATH] = {0};
								u64 BytesWritten = CStringCopy(Current->File.Path.Absolute, NewPath, PLORE_MAX_PATH);
								Assert(BytesWritten < PLORE_MAX_PATH - 1);
								if (!WeAreTopLevel) {
									NewPath[BytesWritten++] = '\\';
								}
								CStringCopy(Movee->File.Path.FilePart, NewPath + BytesWritten, PLORE_MAX_PATH);
								
								b64 DidMoveOk = Platform->MoveFile(Movee->File.Path.Absolute, NewPath);
								if (DidMoveOk) {
									char MoveeCopy[PLORE_MAX_PATH] = {0};
									CStringCopy(Movee->File.Path.Absolute, MoveeCopy, PLORE_MAX_PATH);
									
									RemoveListing(FileContext, Movee);
									
									if (!Platform->IsPathTopLevel(Movee->File.Path.Absolute, PLORE_MAX_PATH)) {
										Platform->PopPathNode(MoveeCopy, PLORE_MAX_PATH, false);
										plore_file_listing *Parent = GetListing(FileContext, MoveeCopy);
										if (Parent) {
											RemoveListing(FileContext, Parent);
										}
									}
									
									PastedCount++;
								} else {
									DrawText("Couldn't paste `%s`", Movee->File.Path.FilePart);
								}
							}
							DrawText("Pasted %d items!", PastedCount);
							
							if (DirectoryState.Current) RemoveListing(FileContext, DirectoryState.Current);
							
							PushVimCommand(State->VimContext, (vim_command) {
											   .State = InteractState_FileExplorer,
											   .Type = VimCommandType_Paste,
											   .Paste = {
												   .PasteeCount = PastedCount,
											   },
										   });
							
							FileContext->YankedCount = 0;
							MemoryClear(FileContext->Yanked, sizeof(FileContext->Yanked));
							
							FileContext->SelectedCount = 0;
							MemoryClear(FileContext->Selected, sizeof(FileContext->Selected));
						}
					} break;
					case VimCommandType_Movement: {
						switch (Command.Movement.Direction) {
							case Left: {
								char Buffer[PLORE_MAX_PATH] = {0};
								Platform->GetCurrentDirectory(Buffer, PLORE_MAX_PATH);
								
								Print("Moving up a directory, from %s ", Buffer);
								Platform->PopPathNode(Buffer, PLORE_MAX_PATH, false);
								PrintLine("to %s", Buffer);
								
								Platform->SetCurrentDirectory(Buffer);
								
								PushVimCommand(State->VimContext, (vim_command) {
												   .State = InteractState_FileExplorer,
												   .Type = VimCommandType_Movement,
												   .Movement = {
													   .Direction = Left,
												   },
											   });
							} break;
							case Right: {
								// move right
								plore_file *CursorEntry = FileContext->Current->Entries + FileContext->Current->Cursor;
							
								if (CursorEntry->Type == PloreFileNode_Directory) {
									PrintLine("Moving down a directory, from %s to %s", 
											  FileContext->Current->File.Path.Absolute, 
											  CursorEntry->Path.Absolute);
									Platform->SetCurrentDirectory(CursorEntry->Path.Absolute);
									
									PushVimCommand(State->VimContext, (vim_command) {
													   .State = InteractState_FileExplorer,
													   .Type = VimCommandType_Movement,
													   .Movement = {
														   .Direction = Right,
													   },
												   });
								} else {
									if (CursorEntry->Extension != PloreFileExtension_Unknown) {
										DrawText("Opening %s", CursorEntry->Path.FilePart);
										Platform->RunShell(PloreFileExtensionHandlers[CursorEntry->Extension], CursorEntry->Path.Absolute);
									} else {
										DrawText("Unknown extension - specify a handler", CursorEntry->Path.FilePart);
									}
								}
							} break;
							
							case Up: {
								if (FileContext->Current->Count) {
									if (FileContext->Current->Cursor == 0) {
										FileContext->Current->Cursor = FileContext->Current->Count-1;
									} else {
										FileContext->Current->Cursor -= 1;
									}
									
									PushVimCommand(State->VimContext, (vim_command) {
													   .State = InteractState_FileExplorer,
													   .Type = VimCommandType_Movement,
													   .Movement = {
														   .Direction = Up,
													   },
												   });
								}
							} break;
							
							case Down: {
									FileContext->Current->Cursor = (FileContext->Current->Cursor + 1) % FileContext->Current->Count;
									PushVimCommand(State->VimContext, (vim_command) {
													   .State = InteractState_FileExplorer,
													   .Type = VimCommandType_Movement,
													   .Movement = {
														   .Direction = Down,
													   },
												   });
							} break;
						} break;
					} break;
					
					case VimCommandType_Select: {
						if (DirectoryState.Cursor) {
							ToggleSelected(FileContext, DirectoryState.Cursor);
							
							PushVimCommand(State->VimContext, (vim_command) {
											   .State = InteractState_FileExplorer,
											   .Type = VimCommandType_Select,
											   .Select = {
												   .SelectCount = 1,
											   },
										   });
						}
					} break;
			
				default: {
				} break;
		}
		AteCommands = true;
				
		} break;
		case InteractState_CommandHistory: {
			PrintLine("Command history mode.");
		} break;
			
		}
	} else if (State->VimContext->CommandKeyCount == State->VimContext->MaxCommandCount) {
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
	f32 X = 10;
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
	
	typedef struct image_preview_handle {
		plore_path Path;
		platform_texture_handle Texture;
		b64 Allocated;
	} image_preview_handle;
	
	if (Window(State->VimguiContext, (vimgui_window_desc) {
				   .Title = FileContext->Current->File.Path.Absolute,
				   .Rect = { .P = V2(0, 0), .Span = { PlatformAPI->WindowDimensions.X, PlatformAPI->WindowDimensions.Y - FooterHeight } },
			   })) {
		
		
#if 1
		if (DirectoryState.Cursor) {
			char CursorInfo[512] = {0};
			StringPrintSized(CursorInfo, 
							 ArrayCount(CursorInfo),
						     "[%s] %s 01-02-3 %s", 
						     (DirectoryState.Cursor->File.Type == PloreFileNode_Directory) ? "DIR" : "FILE", 
						     DirectoryState.Cursor->File.Path.FilePart,
							 PloreKeysToString(&State->FrameArena, LastCommand, LastCommandCount));
			
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
							
							plore_file_listing *RowEntry = GetOrInsertListing(FileContext, ListingFromFile(&Listing->Entries[Row])).Listing;
							if (Listing->Cursor == Row) {
								BackgroundColour = WidgetColour_Secondary;
							} else if (IsYanked(FileContext, RowEntry)) {
								BackgroundColour = WidgetColour_Tertiary;
							} else if (IsSelected(FileContext, RowEntry)) {
								BackgroundColour = WidgetColour_Quaternary;
							}
							
							if (RowEntry->File.Type == PloreFileNode_Directory) {
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
								Listing->Cursor = Row;
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
						if (Listing->File.Extension == PloreFileExtension_BMP) {
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
								}
								MyHandle->Path = Listing->File.Path;
								platform_readable_file File = Platform->DebugOpenFile(MyHandle->Path.Absolute);
								char *Buffer = PushBytes(&State->FrameArena, File.FileSize);
								Platform->DebugReadEntireFile(File, Buffer, File.FileSize);
								loaded_bitmap Bitmap = LoadBMP(Buffer);
								MyHandle->Texture = Platform->CreateTextureHandle((platform_texture_handle_desc) {
																					  .Pixels = Bitmap.Memory, 
																					  .Width = Bitmap.Width, 
																					  .Height = Bitmap.Height,
																					  .ProvidedPixelFormat = PixelFormat_BGRA8,
																					  .TargetPixelFormat = PixelFormat_RGBA8,
																					  .FilterMode = FilterMode_Nearest,
																				  });
								Platform->DebugCloseFile(File);
							}
						}
						
						if (MyHandle) { 
							if (Image(State->VimguiContext, (vimgui_image_desc) { 
											  .ID = (u64) MyHandle,
											  .Texture = MyHandle->Texture,
											  .Rect = {
												  .P = V2(1330, 250),
												  .Span = V2(512, 512),
											  }, 
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
		
		WindowEnd(State->VimguiContext);
	}
	
	if (AteCommands) {
		AteCommands = false;
		
		State->VimContext->CommandKeyCount = 0;
		LastCommandCount = 0;
		MemoryClear(State->VimContext->CommandKeys, sizeof(State->VimContext->CommandKeys));
		MemoryClear(LastCommand, sizeof(LastCommand));
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
	Assert(Selectee);
	
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
	DrawText("There are now %d selected items.", Context->SelectedCount);
}

internal plore_current_directory_state
SynchronizeCurrentDirectory(plore_state *State) {
	plore_current_directory_state CurrentState = {0};
	
	plore_file_context *FileContext = State->FileContext;
	plore_path CurrentDir = Platform->GetCurrentDirectoryPath();
	
	FileContext->Current = GetOrInsertListing(FileContext, ListingFromDirectoryPath(&CurrentDir)).Listing;
	FileContext->InTopLevelDirectory = Platform->IsPathTopLevel(CurrentDir.Absolute, PLORE_MAX_PATH);
	CurrentState.Current = FileContext->Current;
	
	if (FileContext->Current->Count) {
		plore_file *CursorEntry = FileContext->Current->Entries + FileContext->Current->Cursor;
		
		CurrentState.Cursor = GetOrInsertListing(FileContext, ListingFromFile(CursorEntry)).Listing;
	}
	
	if (!FileContext->InTopLevelDirectory) { 
		// CLEANUP(Evan): Doesn't need to copy twice!
		plore_file_listing CurrentCopy = *FileContext->Current;
		Platform->PopPathNode(CurrentCopy.File.Path.Absolute, ArrayCount(CurrentCopy.File.Path.Absolute), false);
		
		plore_file_listing_get_or_insert_result ParentResult = GetOrInsertListing(FileContext, ListingFromDirectoryPath(&CurrentCopy.File.Path));
		
		// Make sure the parent's cursor is set to the current directory.
		// The parent's row will be invalid on plore startup.
		CurrentState.Parent = ParentResult.Listing;
		if (!ParentResult.DidAlreadyExist) {
			for (u64 Row = 0; Row < CurrentState.Parent->Count; Row++) {
				plore_file *File = CurrentState.Parent->Entries + Row;
				if (File->Type != PloreFileNode_Directory) continue;
				
				if (CStringsAreEqual(File->Path.Absolute, FileContext->Current->File.Path.Absolute)) {
					CurrentState.Parent->Cursor = Row;
					break;
				}
			}
			
		}
		
	}
	
	
	return(CurrentState);
}

internal char *
PloreKeysToString(memory_arena *Arena, plore_key *Keys, u64 KeyCount) {
	u64 Size = KeyCount*8+1;
	u64 Count = 0;
	if (!KeyCount || !Keys) return("");
	
	char *Result = PushBytes(Arena, Size);
	char *Buffer = Result; 
	
	for (u64 K = 0; K < KeyCount; K++) {
		Count += CStringCopy(PloreKeyStrings[Keys[K]], Buffer, Size - Count);
		Buffer += Count;
	}
	
	return(Result);
}
	