#include "plore.h"

#if defined(PLORE_INTERNAL)
internal void DrawText(char *Format, ...);
#endif

#include "plore_string.h"
#include "plore_vimgui.h"
#include "plore_vim.h"

global platform_api *Platform;
global platform_debug_print_line *PrintLine;
global platform_debug_print *Print;

#if defined(PLORE_INTERNAL)
#ifdef Assert
#undef Assert
#define Assert(X) if (!(X)) {                                                                                      \
					  char __AssertBuffer[256];                                                                    \
					  StringPrintSized(__AssertBuffer,                                                             \
					  ArrayCount(__AssertBuffer),                                                                  \
						"Assert fired on line %d in file %s\n"                                                     \
						"Try again to attach debugger, abort to exit, continue to ignore.",                        \
					  __LINE__,                                                                                    \
					  __FILE__);                                                                                   \
					  if (Platform->DebugAssertHandler(__AssertBuffer)) Debugger;                                  \
}
#endif
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

platform_texture_handle TextureHandle_CantLoad;
memory_arena *STBIFrameArena = 0;
internal void *
STBIMalloc(u64 Size) {
	void *Result = PushBytes(STBIFrameArena, Size);
	Assert(Result);
	
	return(Result);
}

internal void
STBIFree(void *Ptr) {
	(void)Ptr;
}

internal void *
STBIRealloc(void *Ptr, u64 Size) {
	(void)Ptr;
	
	void *Result = STBIMalloc(Size);
	Assert(Result);
	
	return(Result);
}

#define STBI_MALLOC(Size) STBIMalloc(Size)
#define STBI_FREE(Ptr) STBIFree(Ptr)
#define STBI_REALLOC(Ptr, Size) STBIRealloc(Ptr, Size)
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef enum file_sort_mask {
	FileSort_Size,
	FileSort_Name,
	FileSort_Modified,
	FileSort_Count,
	_FileSort_ForceU64 = 0xffffffffull,
} file_sort_mask;

typedef struct plore_file_filter_state {
	// NOTE(Evan): This is used to temporarily match against files during ISearch, including files processed by TextFilter.
	text_filter ISearchFilter;
	
	// NOTE(Evan): This hard-removes matching/non-matching file entries from the directory listing, persisting for the tab's lifetime.
	// Matching/non-matching is given by the flag below.
	text_filter GlobalListingFilter;
	
	char ExtensionFilter[PLORE_MAX_PATH];
	u64 ExtensionFilterCount;
	
	file_sort_mask SortMask;
	b64 SortAscending;
	
	struct {
		plore_file_extension Extensions[PloreFileExtension_Count];
		plore_file_node FileNodes[PloreFileNode_Count];
		b64 HiddenFiles;
	} HideMask;
	
} plore_file_filter_state;

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


typedef struct plore_tab {
	memory_arena Arena;          // NOTE(Evan): Zeroed when tab is closed.
	plore_path CurrentDirectory; // NOTE(Evan): Needs to be cached, as current directory is set for the entire process.
	plore_current_directory_state *DirectoryState;
	plore_file_filter_state *FilterState;
	plore_file_context *FileContext;
	b64 Active;
} plore_tab;

typedef struct plore_state {
	b64 Initialized;
	f64 DT;
	
	plore_tab Tabs[8];
	u64 TabCount;
	u64 TabCurrent;
	
	plore_memory *Memory;
	plore_vim_context *VimContext;
	plore_vimgui_context *VimguiContext;
	plore_render_list *RenderList;
	
	memory_arena Arena;      // NOTE(Evan): Never freed.
	memory_arena FrameArena; // NOTE(Evan): Freed at the beginning of each frame.
	
	plore_font *Font;
} plore_state;


internal b64
SetCurrentTab(plore_state *State, u64 NewTab);

internal plore_tab *
GetCurrentTab(plore_state *State);

internal void
ClearTab(plore_state *State, u64 TabIndex);
	

typedef struct load_image_result {
	platform_texture_handle Texture;
	b64 LoadedSuccessfully;
	char *ErrorReason;
} load_image_result;

internal load_image_result
LoadImage(memory_arena *Arena, char *AbsolutePath, u64 MaxX, u64 MaxY);

internal void
PrintDirectory(plore_file_listing_info *Directory);

plore_inline b64
IsYanked(plore_file_context *Context, plore_path *Yankee);

plore_inline b64
IsSelected(plore_file_context *Context, plore_path *Selectee);

internal void
ToggleSelected(plore_file_context *Context, plore_path *Selectee);

internal void
ToggleYanked(plore_file_context *Context, plore_path *Yankee);
	
internal char *
PloreKeysToString(memory_arena *Arena, vim_key *Keys, u64 KeyCount);

typedef struct vim_keys_to_string_result {
	char *Buffer;
	u64 BytesWritten;
} vim_keys_to_string_result;
internal vim_keys_to_string_result
VimKeysToString(char *Buffer, u64 BufferSize, vim_key *Keys);
	
internal char *
VimBindingToString(char *Buffer, u64 BufferSize, vim_binding *Binding);
	
internal void
SynchronizeCurrentDirectory(memory_arena *FrameArena, plore_tab *Tab);
	
internal plore_file *
GetCursorFile(plore_state *State);

internal plore_path *
GetImpliedSelection(plore_state *State);
	
internal plore_key
GetKey(char C) {
	plore_key Result = 0;
	for (u64 K = 0; K < ArrayCount(PloreKeyLookup); K++) {
		plore_key_lookup Lookup = PloreKeyLookup[K];
		if (Lookup.C == C || (Lookup.C == ToLower(C))) {
			Result = Lookup.K;
			break;
		} 
	}
	
	return(Result);
}

#if defined(PLORE_INTERNAL)
#include "plore_debug.c"
#endif

#include "plore_memory.c"
#include "plore_table.c"
#include "plore_vim.c"
#include "plore_vimgui.c"
#include "plore_time.c"
#include "plore_tab.c"


internal plore_file *
GetCursorFile(plore_state *State) {
	plore_tab *Tab = GetCurrentTab(State);
	plore_file_listing_info_get_or_create_result CursorResult = GetOrCreateFileInfo(Tab->FileContext, &Tab->DirectoryState->Current.File.Path);
	plore_file *CursorEntry = Tab->DirectoryState->Current.Entries + CursorResult.Info->Cursor;
	
	return(CursorEntry);
}

// @Rename
internal plore_path *
GetImpliedSelection(plore_state *State) {
	plore_path *Result = 0;
	
	plore_tab *Tab = GetCurrentTab(State);
	plore_file_context *FileContext = Tab->FileContext;
	
	if (FileContext->SelectedCount == 1) {
		Result = &FileContext->Selected[0];
	} else if (!FileContext->SelectedCount) {
		plore_file_listing *Cursor = &Tab->DirectoryState->Cursor;
		if (Cursor->Valid) {
			Result = &GetOrCreateFileInfo(FileContext, &Cursor->File.Path).Info->Path;
		}
	}
	
	return(Result);
}

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
		STBIFrameArena = &State->FrameArena;
		
		// MAINTENANCE(Evan): Don't copy the main arena by value until we've allocated the frame arena.
		State->Arena = PloreMemory->PermanentStorage;
		
		for (u64 T = 0; T < ArrayCount(State->Tabs); T++) {
			plore_tab *Tab = State->Tabs + T;
			Tab->Arena = SubArena(&State->Arena, Megabytes(2), 16);
		}
		
		InitTab(State, &State->Tabs[0]);
		
		State->Font = FontInit(&State->Arena, "C:\\plore\\data\\fonts\\Inconsolata-Light.ttf");
		
#if defined(PLORE_INTERNAL)
		DebugInit(State);
#endif
		
		State->VimContext = PushStruct(&State->Arena, plore_vim_context);
		State->VimContext->Mode = VimMode_Normal;
		
		State->RenderList = PushStruct(&State->Arena, plore_render_list);
		State->RenderList->Font = State->Font;
		
		State->VimguiContext = PushStruct(&State->Arena, plore_vimgui_context);
		VimguiInit(State->VimguiContext, State->RenderList);
		
		
		SynchronizeCurrentDirectory(&State->FrameArena, GetCurrentTab(State));
		
	} else {
		ClearArena(&State->FrameArena);
	}
	
	
	
#if defined(PLORE_INTERNAL)
	if (PloreInput->DLLWasReloaded) {
		PlatformInit(PlatformAPI);
		STBIFrameArena = &State->FrameArena;
		DebugInit(State);
	}
#endif
	
	plore_tab *Tab = GetCurrentTab(State);
	
	plore_vim_context *VimContext = State->VimContext;
	keyboard_and_mouse BufferedInput = PloreInput->ThisFrame;
	
	local f32 LastTime;
	State->DT = PloreInput->Time - LastTime;
	LastTime = PloreInput->Time;
	
	// CLEANUP(Evan): We Begin() here as the render list is shared for debug draw purposes right now.
	VimguiBegin(State->VimguiContext, BufferedInput, PlatformAPI->WindowDimensions);
	
	b64 DidInput = false;
	vim_key BufferedKeys[64] = {0};
	u64 BufferedKeyCount = 0;
	
	for (u64 K = 0; K < BufferedInput.TextInputCount; K++) {
		if (VimContext->CommandKeyCount == ArrayCount(VimContext->CommandKeys)) break;
		
		char C = BufferedInput.TextInput[K];
		plore_key PK = GetKey(C);
		
		// NOTE(Evan): Modifiers are mutually exclusive!
		plore_key Modifier = PloreKey_None;
		if (BufferedInput.sKeys[PK]) {
			Modifier = PloreKey_Shift;
		} else if (BufferedInput.cKeys[PK]) {
			Modifier = PloreKey_Ctrl;
		}
			
		vim_pattern Pattern = VimPattern_None;
		if (IsNumeric(C)) {
			Pattern = VimPattern_Digit;
		}
		
		VimContext->CommandKeys[VimContext->CommandKeyCount++] = (vim_key) { 
			.Input = PK, 
			.Modifier = Modifier,
			.Pattern = Pattern,
		};
		
		DidInput = true;
	}

	//
	// NOTE(Evan): Vim command processing
	//
	local make_command_result CommandCandidates = {0};
	make_command_result CommandThisFrame = {0};
	if (DidInput) {
		CommandThisFrame = MakeCommand(VimContext);
		CommandCandidates = CommandThisFrame;
		vim_command Command = CommandThisFrame.Command;
		
		if (Command.Mode) {
			switch (Command.Mode) {
				// NOTE(Evan): We need to cleanup the active command at one point, but it's not clear exactly where.
				// Right now we are just using the mode switch boundary.
				case VimMode_Normal: {
					VimContext->Mode = VimMode_Normal;
					SetActiveCommand(VimContext, Command);
				} break;
				case VimMode_Insert: {
					VimContext->Mode = VimMode_Insert;
					SetActiveCommand(VimContext, Command);
				} break;
				
				case VimMode_Lister: {
					DrawText("Lister");
				} break;
			}
			
			ClearCommands(VimContext);
		}
		
		if (Command.Type) {
			switch (VimContext->Mode) {
				case VimMode_Normal: {
					VimCommands[Command.Type](State, Tab, VimContext, Tab->FileContext, Command);
				} break;
				
				case VimMode_Insert: {
					Assert(VimContext->ActiveCommand.Type);
					
					// NOTE(Evan): Mirror the insert state. 
					VimContext->ActiveCommand.State = Command.State;
					
					switch (Command.State) {
						case VimCommandState_Start: 
						case VimCommandState_Incomplete: {
							VimCommands[VimContext->ActiveCommand.Type](State, Tab, VimContext, Tab->FileContext, VimContext->ActiveCommand);
						} break;
						
						case VimCommandState_Finish: {
							// NOTE(Evan): Copy insertion into the active command, then call the active command function before cleaning up.
							u64 Size = 512;
							VimContext->ActiveCommand.Shell = PushBytes(&State->FrameArena, Size);
							VimKeysToString(VimContext->ActiveCommand.Shell, Size, VimContext->CommandKeys);
							
							
							VimCommands[VimContext->ActiveCommand.Type](State, Tab, VimContext, Tab->FileContext, VimContext->ActiveCommand);
							
							ClearCommands(VimContext);
							VimContext->Mode = VimMode_Normal;
							VimContext->ActiveCommand = ClearStruct(vim_command);
						} break;
						
						InvalidDefaultCase;
					}
				} break;
				
				case VimMode_Lister: {
					switch (Command.State) {
						case VimCommandState_Start:
						case VimCommandState_Incomplete: {
							VimCommands[VimContext->ActiveCommand.Type](State, Tab, VimContext, Tab->FileContext, VimContext->ActiveCommand);
						} break;
						
						case VimCommandState_Finish: {
							VimCommands[VimContext->ActiveCommand.Type](State, Tab, VimContext, Tab->FileContext, VimContext->ActiveCommand);
							ResetVimState(VimContext);
						} break;
						
						InvalidDefaultCase;
					}
				} break;
				
				InvalidDefaultCase;
				
			}
		}
		
		b64 FinalizedCommand = (Command.Type && Command.State != VimCommandState_Incomplete);
		b64 NoPossibleCandidates = (!Command.Type && !CommandThisFrame.CandidateCount);
		b64 NoActiveCommand = !VimContext->ActiveCommand.Type;
		if ((FinalizedCommand || (NoPossibleCandidates && Command.State != VimCommandState_Incomplete)) && NoActiveCommand) {
			ClearCommands(VimContext);
		}
	}
	
	SynchronizeCurrentDirectory(&State->FrameArena, GetCurrentTab(State));
	
	Tab = GetCurrentTab(State);
	
	//
	// NOTE(Evan): GUI stuff.
	//
	u64 Cols = 3;
	
	f32 FileRowHeight = 36.0f;
	f32 FooterPad = 0;
	f32 FooterHeight = 60;
	f32 fCols = (f32) Cols;
	f32 PadX = 10.0f;
	f32 PadY = 10.0f;
	f32 StartW = ((PlatformAPI->WindowDimensions.X  - 0)            - (fCols + 1) * PadX) / fCols;
	f32 StartH = ((PlatformAPI->WindowDimensions.Y - FooterHeight)  - (1     + 1) * PadY) / 1;
	f32 W = StartW;
	f32 H = StartH;
	f32 X = 10;
	f32 Y = 0;
	
	
	
	typedef struct plore_viewable_directory {
		plore_file_listing *File;
		b64 Focus;
	} plore_viewable_directory;
	
	plore_viewable_directory ViewDirectories[3] = {
		[0] = {
			.File = !Tab->FileContext->InTopLevelDirectory ? ViewDirectories[0].File = &Tab->DirectoryState->Parent : 0,
		},
		[1] = {
			.File = &Tab->DirectoryState->Current,
			.Focus = true,
		}, 
		[2] = {
			.File = Tab->DirectoryState->Cursor.Valid ? ViewDirectories[2].File = &Tab->DirectoryState->Cursor : 0,
		},
		
	};
	
	typedef struct image_preview_handle {
		plore_path Path;
		platform_texture_handle Texture;
		b64 Allocated;
	} image_preview_handle;
	
	b64 StoleFocus = false;
	
	if (Window(State->VimguiContext, (vimgui_window_desc) {
					   .ID = (u64) Tab->DirectoryState->Current.File.Path.Absolute,
					   .Rect = { 
						   .P = V2(0, 0), 
						   .Span = { 
							   PlatformAPI->WindowDimensions.X, 
							   PlatformAPI->WindowDimensions.Y - FooterHeight 
						   } 
					   },
					   .BackgroundColour      = WidgetColour_Window,
					   .BackgroundColourFlags = WidgetColourFlags_Default,
					   .NeverFocus = true,
				   })) {
		
		// NOTE(Evan): Tabs.
		f32 TabWidth = 240;
		f32 TabHeight = 48;
		u64 TabCount = 0;
		
		plore_tab *Active = GetCurrentTab(State);
		for (u64 T = 0; T < ArrayCount(State->Tabs); T++) {
			plore_tab *Tab = State->Tabs + T;
			if (Tab->Active) {
				TabCount++;
				
				widget_colour_flags BackgroundColourFlags = WidgetColourFlags_Default;
				widget_colour BackgroundColour = WidgetColour_Default;
				
				if (Tab == Active) {
					BackgroundColourFlags = WidgetColourFlags_Focus;
				}
				
				u64 NumberSize = 32;
				char *Number = PushBytes(&State->FrameArena, NumberSize);
				StringPrintSized(Number, NumberSize, "%d", T+1);
				
				if (Button(State->VimguiContext, (vimgui_button_desc) {
								   .ID = (u64) Tab,
								   .Rect = { 
									   .P =    { (TabCount)*PadX + (TabCount-1)*TabWidth, PadY },
									   .Span = { TabWidth, TabHeight },
								   },
								   .Title = {
									   .Text = Tab->CurrentDirectory.FilePart, 
									   .Colour = (Tab == Active) ? TextColour_TabActive : TextColour_Tab,
									   .Pad = V2(0, 6),
									   .Alignment = VimguiLabelAlignment_Center,
								   },
								   .Secondary = {
									   .Text = Number,
									   .Alignment = VimguiLabelAlignment_Left,
									   .Colour = (Tab == Active) ? TextColour_TabActive : TextColour_Tab,
									   .Pad = V2(10, 6),
								   },
								   .BackgroundColourFlags = BackgroundColourFlags,
								   .BackgroundColour = BackgroundColour,
							   })) {
					// TODO(Evan): Switch tab if clicked!
					SetCurrentTab(State, T);
				}
			}
		}
		
		if (State->TabCount) {
			Y += TabHeight;
			H -= TabHeight;
		}
		// NOTE(Evan): Cursor state.
		local f64 Tick = 0;
		local b64 DoBlink = false; 
		
		Tick += PloreInput->DT;
		if (Tick > 0.3f) {
			DoBlink = !DoBlink;
			Tick = 0;
		}
		
		if (DidInput) {
			DoBlink = true;
			Tick = 0;
		}
		
		
		// NOTE(Evan): Interactive Mode, appears at bottom of screen.
#if 1
		if (VimContext->Mode == VimMode_Insert) {
			char *InsertPrompt = VimInsertPrompts[VimContext->ActiveCommand.Type];
			if (!InsertPrompt) InsertPrompt = "YOU SHOULD NOT SEE THIS.";
			char Buffer[128] = {0};
			u64 BufferSize = 0;
			
			u64 Size = 128;
			char *S = PushBytes(&State->FrameArena, Size);
			BufferSize += StringPrintSized(Buffer, ArrayCount(Buffer), "%s ", InsertPrompt);
			BufferSize += StringPrintSized(Buffer+BufferSize, ArrayCount(Buffer), 
										   "%s", 
										   VimKeysToString(S, Size, VimContext->CommandKeys).Buffer);
			
			H -= (FooterHeight + 2*PadY);
			
			StoleFocus = true;
			
			if (Button(State->VimguiContext, (vimgui_button_desc) {
							   .ID = (u64) Buffer,
							   .Title = {
								   .Text = Buffer, 
								   .Colour = TextColour_Prompt,
								   .Pad = V2(16, 12),
								   .Alignment = VimguiLabelAlignment_Left,
							   },
							   .Secondary = {
								   .Text = DoBlink ? "|" : "",
								   .Colour = TextColour_PromptCursor,
								   .Alignment = VimguiLabelAlignment_Left,
								   .Pad = V2(-4, 12),
							   },
								   
							   .Rect = { 
								   .P    = V2(PadX, PlatformAPI->WindowDimensions.Y - 2*FooterHeight - 2*PadY), 
								   .Span = V2(PlatformAPI->WindowDimensions.X-2*PadX, FooterHeight + PadY)
							   },
							   .BackgroundColourFlags = WidgetColourFlags_Focus,
						   })) {
			}
		} else if (VimContext->Mode == VimMode_Lister && VimContext->ActiveCommand.Type == VimCommandType_OpenFile) {
			plore_file_listing *Listing = &Tab->DirectoryState->Cursor;
			plore_handler *Handlers = PloreFileExtensionHandlers[Listing->File.Extension];
			u64 HandlerCount = 0;
			
			// NOTE(Evan): We creates widgets bottom-to-top, so the offset corrects for that.
			for (u64 Handler = 0; Handler < PLORE_FILE_EXTENSION_HANDLER_MAX; Handler++) {
				u64 Offset = (PLORE_FILE_EXTENSION_HANDLER_MAX-Handler-1);
				plore_handler *TheHandler = Handlers + Offset;
				if (TheHandler->Shell) {
					b64 IsOnCursor = Offset == VimContext->ListerCursor;
					
					HandlerCount++;
					u64 HandlerSize = 512;
					char *HandlerString = PushBytes(&State->FrameArena, HandlerSize);
					StringPrintSized(HandlerString, HandlerSize, "%-30s %-128s",
									 TheHandler->Name,
									 TheHandler->Shell
									 );
					
					u64 ID = (u64) TheHandler->Shell;
					
					if (Button(State->VimguiContext, (vimgui_button_desc) {
								   .ID = ID,
								   .Title = {
									   .Text = HandlerString,
									   .Pad = V2(16, 8),
								   },
								   .Rect = {
									   .P = V2(PadX, PlatformAPI->WindowDimensions.Y - FooterHeight - HandlerCount*FooterHeight - 20),
									   .Span = V2(PlatformAPI->WindowDimensions.X-2*PadX, FooterHeight + PadY)
								   },
								   .BackgroundColour = IsOnCursor ? WidgetColour_Tertiary : WidgetColour_Primary,
							   })) {
					}
					
					H -= FooterHeight;
				}
			}
			
			H -= 20;
			
			StoleFocus = true;
		}
#endif
		
		// NOTE(Evan): Draw cursor info or candidate list!
		
		// NOTE(Evan): If we didn't eat the command and there are candidates, list them!
		if (VimContext->Mode == VimMode_Normal && VimContext->CommandKeyCount && CommandCandidates.CandidateCount) {
			u64 CandidateCount = 0;
			for (u64 C = 0; C < ArrayCount(CommandCandidates.Candidates); C++) {
				vim_binding *Candidate = 0;
				if (CommandCandidates.Candidates[C]) {
					Candidate = VimBindings + C;
					CandidateCount++;
				}
				
				if (Candidate) {
					u64 BindingSize = 32;
					char *BindingString = PushBytes(&State->FrameArena, BindingSize);
					BindingString = VimBindingToString(BindingString, BindingSize, Candidate);
					
					u64 CandidateSize = 256;
					char *CandidateString = PushBytes(&State->FrameArena, CandidateSize);
					
					StringPrintSized(CandidateString, CandidateSize, "%-8s %-28s %-32s",
									 BindingString,
									 VimCommandStrings[Candidate->Type],
									 (Candidate->Shell ? Candidate->Shell : ""));
					
					u64 ID = Candidate->Shell ? (u64) Candidate->Shell : (u64) CandidateString + Candidate->Type;
					
					if (Button(State->VimguiContext, (vimgui_button_desc) {
									   .ID = ID,
									   .Title = {
										   .Text = CandidateString,
										   .Pad = V2(16, 16),
									   },
									   .Rect = {
										   .P = V2(PadX, PlatformAPI->WindowDimensions.Y - FooterHeight - CandidateCount*(FooterHeight + PadY) + PadY - 20),
										   .Span = V2(PlatformAPI->WindowDimensions.X-2*PadX, FooterHeight + PadY)
									   },
									   .BackgroundColourFlags = WidgetColourFlags_Focus,
								   })) {
						ClearCommands(State->VimContext);
						vim_command Command = {
						    .Type = Candidate->Type,
							.Shell = Candidate->Shell,
							.Scalar = 1,
							.State = VimCommandState_Start,
						};
						VimCommands[Command.Type](State, Tab, State->VimContext, Tab->FileContext, Command);
					}
					
					H -= FooterHeight + PadY;
				}
			}
			
			H -= 20-PadY;
			
			StoleFocus = true;
		}
		
		
		// NOTE(Evan): Cursor information, appears at bottom of screen.
		
		char CursorInfo[512] = {0};
		char *CommandString = "";
		
		// NOTE(Evan): Only show incomplete command key buffer!
		switch (VimContext->Mode) {
			case VimMode_Normal: {
				CommandString = PloreKeysToString(&State->FrameArena, VimContext->CommandKeys, VimContext->CommandKeyCount);
			} break;
		}
		
		// NOTE(Evan): Prompt string.
		char *FilterText = 0;
		if (Tab->FilterState->GlobalListingFilter.TextCount) {
			u64 FilterTextSize = 256;
			
			FilterText = PushBytes(&State->FrameArena, FilterTextSize);
			StringPrintSized(FilterText, FilterTextSize, "Filter: %s ", Tab->FilterState->GlobalListingFilter.Text);
		}
		
		u64 BufferSize = 256;
		char *Buffer = PushBytes(&State->FrameArena, BufferSize);
		b64 ShowOrBlink = DoBlink || VimContext->CommandKeyCount || VimContext->Mode != VimMode_Normal || DidInput;
		StringPrintSized(Buffer, BufferSize, "%s%s%s", (FilterText ? FilterText : ""), (ShowOrBlink ? ">>" : ""), CommandString);
		
		
		if (Tab->DirectoryState->Cursor.Valid) {
			plore_file *CursorFile = &Tab->DirectoryState->Cursor.File;
			StringPrintSized(CursorInfo, 
							 ArrayCount(CursorInfo),
						     "[%s] %s %s", 
						     (CursorFile->Type == PloreFileNode_Directory) ? "directory" : "file", 
							 CursorFile->Path.FilePart,
							 PloreTimeFormat(&State->FrameArena, CursorFile->LastModification, "%a %b %d %x")
							 );
		} else {
			StringPrintSized(CursorInfo, 
							 ArrayCount(CursorInfo),
						     "[no selection]"
							 );
		}
		
		if (Button(State->VimguiContext, (vimgui_button_desc) {
						   .Title     = { 
							   .Text = CursorInfo, 
							   .Alignment = VimguiLabelAlignment_Left, 
							   .Colour = TextColour_CursorInfo,
							   .Pad = V2(8, 6),
						   },
						   .Secondary = { 
							   .Text = Buffer,
							   .Alignment = VimguiLabelAlignment_Left,
							   .Pad = V2(0, 6),
							   .Colour = TextColour_Prompt
						   },
						   .Rect = { 
							   .P    = V2(PadX, Y + PlatformAPI->WindowDimensions.X + FooterHeight + FooterPad), 
							   .Span = V2(PlatformAPI->WindowDimensions.X-2*PadX, FooterHeight + PadY-20)
						   },
				   })) {
		}
		
		
		for (u64 Col = 0; Col < Cols; Col++) {
			v2 P      = V2(X, Y);
			v2 Span   = V2(W-3, H-24);
			
			plore_viewable_directory *Directory = ViewDirectories + Col;
			plore_file_listing *Listing = Directory->File;
			if (!Listing) continue; /* Parent can be null, if we are currently looking at a top-level directory. */
			plore_file_listing_info *RowCursor = GetInfo(Tab->FileContext, Listing->File.Path.Absolute);
			char *Title = Listing->File.Path.FilePart;
			
			widget_colour_flags WindowColourFlags = WidgetColourFlags_Default;
			if (Directory->Focus) {
				if (!StoleFocus) {
					WindowColourFlags = WidgetColourFlags_Focus;
				} else {
					WindowColourFlags = WidgetColourFlags_Hot;
				}
			}
			if (Window(State->VimguiContext, (vimgui_window_desc) {
							   .Title                 = Title,
							   .Rect                  = {P, Span}, 
							   .BackgroundColour      = WidgetColour_Primary,
							   .BackgroundColourFlags = WindowColourFlags,
						   })) {
				u64 PageMax = (u64) Floor(H / FileRowHeight)-1;
				u64 Cursor = 0;
				u64 RowStart = Cursor;
				u64 RowEnd = Listing->Count;
				if (RowCursor) {
					Cursor = RowCursor->Cursor;
					u64 Page = (u64) (Cursor / PageMax);
					RowStart = Page*PageMax;
					
					
					u64 RowsAfter = Min(Cursor+PageMax, Listing->Count);
					RowEnd = Clamp(Cursor + RowsAfter, 0, Listing->Count);
				}
				
				switch (Listing->File.Type) {
					case PloreFileNode_Directory: {
						for (u64 Row = RowStart; Row < RowEnd; Row++) {
							plore_file *RowEntry = Listing->Entries + Row;
							
							widget_colour BackgroundColour = WidgetColour_RowPrimary;
							widget_colour_flags WidgetColourFlags = WidgetColourFlags_Default;
							text_colour TextColour = 0;
							text_colour_flags TextColourFlags = 0;
							
							b64 CursorHover = RowCursor && RowCursor->Cursor == Row;
							if (CursorHover) WidgetColourFlags = WidgetColourFlags_Focus;
							
							if (IsYanked(Tab->FileContext, &RowEntry->Path)) {
								BackgroundColour = WidgetColour_RowSecondary;
							} else if (IsSelected(Tab->FileContext, &RowEntry->Path)) {
								BackgroundColour = WidgetColour_RowTertiary;
							} 
							// TODO(Evan): Hidden files could have a slightly different colour while we're viewing them.
							
							
							b64 PassesFilter = true;
							if (Tab->FilterState->ISearchFilter.TextCount) {
								PassesFilter = SubstringNoCase(RowEntry->Path.FilePart, Tab->FilterState->ISearchFilter.Text).IsContained;
							}
							
							
							
							if (RowEntry->Type == PloreFileNode_Directory) {
								TextColour = TextColour_Primary;								
								if (!PassesFilter) TextColourFlags = TextColourFlags_Fade;
							} else {
								TextColour = TextColour_Secondary;								
								if (!PassesFilter) TextColourFlags = TextColourFlags_Fade;
							}
							
							char *Timestamp = PloreTimeFormat(&State->FrameArena, RowEntry->LastModification, "%b %x");
							
							char *SecondaryText = Timestamp;
							if (RowEntry->Type == PloreFileNode_File) {
								char *EntrySizeLabel = " b";
								u64 EntrySize = RowEntry->Bytes;
								if (EntrySize > Megabytes(1)) {
									EntrySize /= Megabytes(1); 
									EntrySizeLabel = "mB";
								} else if (EntrySize > Kilobytes(1)) {
									EntrySize /= Kilobytes(1);
									EntrySizeLabel = "kB";
								}
								
								u64 Size = 256;
								SecondaryText = PushBytes(&State->FrameArena, Size);
								StringPrintSized(SecondaryText, Size, "%s %4d%s", Timestamp, EntrySize, EntrySizeLabel);
							} else {
								u64 Size = 256;
								SecondaryText = PushBytes(&State->FrameArena, Size);
								StringPrintSized(SecondaryText, Size, "%s%7s", Timestamp, "-");
							}
							
							b64 DisplayFileNumbers = false;
							u64 FileNameSize = 256;
							char *FileName = PushBytes(&State->FrameArena, FileNameSize);
							if (DisplayFileNumbers) {
								StringPrintSized(FileName, FileNameSize, "%-3d %s", Row, Listing->Entries[Row].Path.FilePart);
							} else {
								StringPrintSized(FileName, FileNameSize, "%s", Listing->Entries[Row].Path.FilePart);
							}
								
							if (Button(State->VimguiContext, (vimgui_button_desc) {
											   .Title     = { 
												   .Text = FileName,
												   .Alignment = VimguiLabelAlignment_Left ,
												   .Colour = TextColour,
												   .Pad = V2(4, 0),
												   .ColourFlags = TextColourFlags,
											   },
											   .Secondary = { 
												   .Text = SecondaryText, 
												   .Alignment = VimguiLabelAlignment_Right,
												   .Colour = TextColour_Tertiary,
												   .ColourFlags = TextColourFlags,
											   },
											   .FillWidth = true,
											   .BackgroundColour = BackgroundColour,
											   .BackgroundColourFlags = WidgetColourFlags,
											   .Rect = { 
												   .Span = 
												   { 
													   .H = FileRowHeight, 
												   } 
											   },
										   })) {
								if (Listing->Entries[Row].Type == PloreFileNode_Directory) {
									Platform->SetCurrentDirectory(Listing->Entries[Row].Path.Absolute);
								} else {
									Platform->SetCurrentDirectory(Listing->File.Path.Absolute);
								}
								plore_file_listing_info_get_or_create_result CursorResult = GetOrCreateFileInfo(Tab->FileContext, &Listing->File.Path);
								CursorResult.Info->Cursor = Row;
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
		
		WindowEnd(State->VimguiContext);
	}
	
	
#if defined(PLORE_INTERNAL)
	FlushText();
#endif
	
	
	VimguiEnd(State->VimguiContext);
	
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
		if (CStringsAreEqual(It->Absolute, Selectee->Absolute)) {
			return(true);
		}
	}
	
	return(false);
}

internal void
ToggleHelper(plore_file_context *Context, plore_path *Togglee, plore_path *List, u64 ListMax, u64 *ListCount) {
	// NOTE(Evan): Toggle and exit if its in the list.
	for (u64 S = 0; S < *ListCount; S++) {
		plore_path *It = List + S;
		if (CStringsAreEqual(It->Absolute, Togglee->Absolute)) {
			List[S] = List[--(*ListCount)];
			return;
		}
	}
	
	if (*ListCount == ListMax-1) return;
	
	// NOTE(Evan): Otherwise, add it to the list.
	List[(*ListCount)++] = *Togglee;
}

internal void
ToggleSelected(plore_file_context *Context, plore_path *Selectee) {
	Assert(Selectee);
	ToggleHelper(Context, Selectee, Context->Selected, ArrayCount(Context->Selected), &Context->SelectedCount);
}

internal void
ToggleYanked(plore_file_context *Context, plore_path *Yankee) {
	Assert(Yankee);
	ToggleHelper(Context, Yankee, Context->Yanked, ArrayCount(Context->Yanked), &Context->YankedCount);
}


// @Cleanup
internal void
CreateFileListing(plore_file_listing *Listing, plore_file_listing_desc Desc) {
	Assert(Listing);
	
	Listing->File.Type = Desc.Type;
	Listing->File.LastModification = Desc.LastModification;
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


plore_inline b64 
PloreFileSortHelper(plore_file *A, plore_file *B, file_sort_mask SortMask, b64 Ascending) {
	b64 Result = false;
	
	switch (SortMask) {
		case FileSort_Name: {
			Result = StringCompare(A->Path.Absolute, B->Path.Absolute);
		} break;
		
		case FileSort_Modified: {
			Result = A->LastModification.T > B->LastModification.T;
		} break;
		
		case FileSort_Size: {
			Result = A->Bytes > B->Bytes;
		} break;
	}
	
	if (Ascending) Result = !Result;
	return(Result);
}

plore_inline b64
ExtensionIsFiltered(plore_file_extension *List, plore_file_extension Extension) {
	for (plore_file_extension E = 0; E < PloreFileExtension_Count; E++) if (List[E] && (Extension == E)) return(true);
	return(false);
}

plore_inline b64
FileNodeIsFiltered(plore_file_node *List, plore_file_node FileNode) {
	for (plore_file_node F = 0; F < PloreFileExtension_Count; F++) if (List[F] && (FileNode == F)) return(true);
	return(false);
}

internal char *
RemoveSeparators(char *S, u64 Size) {
	char *Current = S;
	while (*Current) {
		char *Tail = Current;
		
		if (*Tail == '_' ||
			*Tail == '-' ||
			*Tail == ' ') {
			u64 SourceLength = StringLength(Tail+1);
			u64 DestLength = StringLength(Tail);
			
			if (SourceLength) CStringCopy(Tail+1, Tail, DestLength);
			else *Tail = '\0';
		} else {
			Current++;
		}
		
	}
	
	return(S);
}


internal b64
StringMatchesFilter(char *S, char *Pattern, text_filter *Filter) {
	b64 Result = false;
	switch (Filter->State) {
		case TextFilterState_Hide: {
			Result = SubstringNoCase(S, Pattern).IsContained;
		} break;
		
		case TextFilterState_Show: {
			Result = !SubstringNoCase(S, Pattern).IsContained;
		} break;
	}
	
	return(Result);
}

internal void
FilterFileListing(memory_arena *Arena, plore_tab *Tab, plore_file_listing *Listing) {
	text_filter *TabFilter = 0;
	char TabTextFilterPattern[PLORE_MAX_PATH] = {0};
	if (Tab->FilterState->GlobalListingFilter.TextCount) {
		TabFilter = &Tab->FilterState->GlobalListingFilter;
		CStringCopy(TabFilter->Text, TabTextFilterPattern, ArrayCount(TabTextFilterPattern));
		RemoveSeparators(TabTextFilterPattern, ArrayCount(TabTextFilterPattern));
	}
	
	text_filter *DirectoryFilter = 0;
	char DirectoryTextFilterPattern[PLORE_MAX_PATH] = {0};
	plore_file_listing_info *Info = GetOrCreateFileInfo(Tab->FileContext, &Listing->File.Path).Info;
	
	if (Info->Filter.TextCount) {
		DirectoryFilter = &Info->Filter;
		CStringCopy(DirectoryFilter->Text, DirectoryTextFilterPattern, ArrayCount(DirectoryTextFilterPattern));
		RemoveSeparators(DirectoryTextFilterPattern, ArrayCount(DirectoryTextFilterPattern));
	}
	
	plore_file *ListingsToKeep = PushArray(Arena, plore_file, Listing->Count);
	u64 ListingsToKeepCount = 0;
	b64 HasTextFilter = TabFilter || DirectoryFilter;
	
	for (u64 F = 0; F < Listing->Count; F++) {
		plore_file *File = Listing->Entries + F;
		
		b64 Discard = 
		    ExtensionIsFiltered(Tab->FilterState->HideMask.Extensions, File->Extension)            ||
			FileNodeIsFiltered(Tab->FilterState->HideMask.FileNodes,   File->Type)                 ||
		   (File->Hidden && Tab->FilterState->HideMask.HiddenFiles);
		
		if (HasTextFilter) {
			char FileNamePattern[PLORE_MAX_PATH] = {0};
			CStringCopy(File->Path.FilePart, FileNamePattern, ArrayCount(FileNamePattern));
			RemoveSeparators(FileNamePattern, ArrayCount(FileNamePattern));
			
			if (TabFilter)       Discard = Discard || StringMatchesFilter(FileNamePattern, TabTextFilterPattern, TabFilter);
			if (DirectoryFilter) Discard = Discard || StringMatchesFilter(FileNamePattern, DirectoryTextFilterPattern, DirectoryFilter);
			 
		 }
			
		if (!Discard) ListingsToKeep[ListingsToKeepCount++] = *File;
	}
	
	StructArrayCopy(ListingsToKeep, Listing->Entries, ListingsToKeepCount, plore_file);
	
	Listing->Count = ListingsToKeepCount;
}

internal void
SynchronizeCurrentDirectory(memory_arena *FrameArena, plore_tab *Tab) {
	plore_file_context *FileContext = Tab->FileContext;
	plore_current_directory_state *CurrentState = Tab->DirectoryState;
	plore_file_filter_state *FilterState = Tab->FilterState; 
	Tab->CurrentDirectory = Platform->GetCurrentDirectoryPath();
	
	CurrentState->Cursor = ClearStruct(plore_file_listing);
	CurrentState->Current = ClearStruct(plore_file_listing);
	
#define PloreSortPredicate(A, B) PloreFileSortHelper(&A, &B, Tab->FilterState->SortMask, Tab->FilterState->SortAscending)
	
	CreateFileListing(&CurrentState->Current, ListingFromDirectoryPath(&Tab->CurrentDirectory));
	FileContext->InTopLevelDirectory = Platform->IsPathTopLevel(CurrentState->Current.File.Path.Absolute, PLORE_MAX_PATH);
	FilterFileListing(FrameArena, Tab, &CurrentState->Current);
	
	if (CurrentState->Current.Count) PloreSort(CurrentState->Current.Entries, CurrentState->Current.Count, plore_file)
	
	if (CurrentState->Current.Valid && CurrentState->Current.Count) {
		plore_file_listing_info_get_or_create_result CursorResult = GetOrCreateFileInfo(FileContext, &CurrentState->Current.File.Path);
		
		
		plore_file *CursorEntry = CurrentState->Current.Entries + CursorResult.Info->Cursor;
		
		
		CreateFileListing(&CurrentState->Cursor, ListingFromFile(CursorEntry));
		FilterFileListing(FrameArena, Tab, &CurrentState->Cursor);
		if (CurrentState->Cursor.Count) PloreSort(CurrentState->Cursor.Entries, CurrentState->Cursor.Count, plore_file)
	}
	
	if (!FileContext->InTopLevelDirectory) {
		// CLEANUP(Evan): Doesn't need to copy twice!
		plore_file_listing CurrentCopy = CurrentState->Current;
		char *FilePart = Platform->PopPathNode(CurrentCopy.File.Path.Absolute, ArrayCount(CurrentCopy.File.Path.Absolute), false).FilePart;
		CStringCopy(FilePart, CurrentCopy.File.Path.FilePart, ArrayCount(CurrentCopy.File.Path.FilePart));
		
		if (Platform->IsPathTopLevel(CurrentCopy.File.Path.Absolute, PLORE_MAX_PATH)) {
			CStringCopy(CurrentCopy.File.Path.Absolute, CurrentCopy.File.Path.FilePart, PLORE_MAX_PATH);
		}
		CreateFileListing(&CurrentState->Parent, ListingFromDirectoryPath(&CurrentCopy.File.Path));
		
		plore_file_listing_info_get_or_create_result ParentResult = GetOrCreateFileInfo(FileContext, &CurrentState->Parent.File.Path);
		FilterFileListing(FrameArena, Tab, &CurrentState->Parent);
		if (CurrentState->Parent.Count) PloreSort(CurrentState->Parent.Entries, CurrentState->Parent.Count, plore_file)
		
		// Make sure the parent's cursor is set to the current directory.
		// The parent's row will be invalid on plore startup.
		if (!ParentResult.DidAlreadyExist) {
			for (u64 Row = 0; Row < CurrentState->Parent.Count; Row++) {
				plore_file *File = CurrentState->Parent.Entries + Row;
				if (File->Type != PloreFileNode_Directory) continue;
				
				if (CStringsAreEqual(File->Path.Absolute, CurrentState->Current.File.Path.Absolute)) {
					ParentResult.Info->Cursor = Row;
					break;
				}
			}
			
		}
		
		
	} else {
		CurrentState->Parent.Valid = false;
	}
	
#undef PloreSortPredicate
	
}

internal char *
PloreKeysToString(memory_arena *Arena, vim_key *Keys, u64 KeyCount) {
	u64 Size = KeyCount*PLORE_KEY_STRING_SIZE+1;
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

// NOTE(Evan): Does not skip unprintable characters.
internal vim_keys_to_string_result
VimKeysToString(char *Buffer, u64 BufferSize, vim_key *Keys) {
	vim_keys_to_string_result Result = {
		.Buffer = Buffer,
	};
	
	char *S = Buffer;
	vim_key *Key = Keys;
	u64 Count = 0;
	Assert(BufferSize >= 3);
	while (Key->Input) {
		char C = PloreKeyCharacters[Key->Input];
		if (Key++->Modifier == PloreKey_Shift && IsAlpha(C)) C = ToUpper(C); 
		// NOTE(Evan): Escape printf specifier!
		if (C == '%' && Count < BufferSize-2) {
			*S++ = '%', Result.BytesWritten++;
		}
		*S++ = C, Result.BytesWritten++;
		if (Count++ == BufferSize-2) break;
	}
	*S = '\0';
	
	
	return(Result);
}

// NOTE(Evan): Does not skip unprintable characters.
// TODO(Evan): Another reason for metaprogram-generated bindings - we could bake this.
internal char *
VimBindingToString(char *Buffer, u64 BufferSize, vim_binding *Binding) {
	char *S = Buffer;
	vim_key *Key = Binding->Keys;
	u64 Count = 0;
	while (Key->Input) {
		char C = PloreKeyCharacters[Key->Input];
		if (Key->Modifier == PloreKey_Shift) {
			C = ToUpper(C);
		}
		*S++ = C;
		Count++;
		Key++;
		if (Count == BufferSize-1) break;
	}
	
	return(Buffer);
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
