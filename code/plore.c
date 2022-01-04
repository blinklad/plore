#include "plore.h"

#if defined(PLORE_INTERNAL)
internal void DrawText(char *Format, ...);
#endif


#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define PLORE_BAKED_FONT_IMPLEMENTATION
#include "generated/plore_baked_font.h"

#include "plore_string.h"
#include "plore_vimgui.h"
#include "plore_vim.h"

global platform_api *Platform;
global platform_debug_print_line *PrintLine;
global platform_debug_print *Print;

#if defined(PLORE_INTERNAL)
#if defined(PLORE_WINDOWS)
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
#endif

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
	FileSort_Extension,
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
	
	b64 HideFileMetadata;
	
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

enum { PloreTab_Count = 8 }; 

typedef struct plore_state {
	b64 Initialized;
	b64 ShouldQuit;
	f64 DT;
	
	plore_tab Tabs[PloreTab_Count];
	u64 TabCount;
	u64 TabCurrent;
	u64 SplitTabs[PloreTab_Count];
	u64 SplitTabCount;
	
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

// NOTE(Evan): Called by SetCurrentTab internally.
internal void
InitTab(plore_state *State, plore_tab *Tab);

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

internal f32
GetCurrentFontHeight(plore_font *Font);

internal f32
GetCurrentFontWidth(plore_font *Font);

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

internal f32
GetCurrentFontHeight(plore_font *Font) {
	f32 Result = Font->Data[Font->CurrentFont]->Height;
	return(Result);
}

// NOTE(Evan): Requires a monospace font.
internal f32
GetCurrentFontWidth(plore_font *Font) {
	f32 Result = Font->Data[Font->CurrentFont]->Data[0].xadvance;
	return(Result);
}

internal plore_font * 
FontInit(memory_arena *Arena, char *Path) {
	f32 DefaultHeight = 32.0f; 
	
	plore_font *Result = PushStruct(Arena, plore_font);
	Result->CurrentFont = 0;
	
	for (u64 F = 0; F < PloreBakedFont_Count; F++) {
		Result->Data[F] = BakedFontData + F;
		Result->Bitmaps[F] = BakedFontBitmaps + F;
		Result->Handles[F] = Platform->CreateTextureHandle((platform_texture_handle_desc) {
															   .Pixels = Result->Bitmaps[F]->Bitmap, 
															   .Width = Result->Bitmaps[F]->BitmapWidth, 
															   .Height = Result->Bitmaps[F]->BitmapHeight, 
															   .TargetPixelFormat = PixelFormat_ALPHA,
															   .ProvidedPixelFormat = PixelFormat_ALPHA,
															   .FilterMode = FilterMode_Linear
														   });
		
		if (Result->Data[F]->Height == DefaultHeight) Result->CurrentFont = F;
	}
	
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
		State->SplitTabs[State->SplitTabCount++] = 0;
		
		State->Font = FontInit(&State->Arena, "data/fonts/Inconsolata-Light.ttf");
		
#if defined(PLORE_INTERNAL)
		DebugInit(State);
#endif
		
		State->VimContext = PushStruct(&State->Arena, plore_vim_context);
		State->VimContext->Mode = VimMode_Normal;
		
		State->RenderList = PushStruct(&State->Arena, plore_render_list);
		State->RenderList->Font = State->Font;
		
		State->VimguiContext = PushStruct(&State->Arena, plore_vimgui_context);
		VimguiInit(&State->Arena, State->VimguiContext, State->RenderList);
		
		
		SynchronizeCurrentDirectory(&State->FrameArena, GetCurrentTab(State));
		
	} else {
		ClearArena(&State->FrameArena);
	}
	
#if defined(PLORE_INTERNAL)
	if (PloreInput->DLLWasReloaded) {
		PlatformInit(PlatformAPI);
		STBIFrameArena = &State->FrameArena;
		DebugInit(State);
		
		for (u64 F = 0; F < PloreBakedFont_Count; F++) {
			State->Font->Data[F] = BakedFontData + F;
			State->Font->Bitmaps[F] = BakedFontBitmaps + F;
		}
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
	
	// @Hack, fix this up once we've implemented F-keys as well.
	if (BufferedInput.pKeys[PloreKey_Escape] && VimContext->CommandKeyCount < ArrayCount(VimContext->CommandKeys)) {
		VimContext->CommandKeys[VimContext->CommandKeyCount++] = (vim_key) { 
			.Input = PloreKey_Escape,
		};
		DidInput = true;
	}
	
	for (u64 K = 0; K < BufferedInput.TextInputCount; K++) {
		if (VimContext->CommandKeyCount == ArrayCount(VimContext->CommandKeys)) break;
		
		char C = BufferedInput.TextInput[K];
		plore_key PK = GetKey(C);
		
		// NOTE(Evan): Modifiers are mutually exclusive!
		// NOTE(Evan): Shift isn't tracked for non-alphanumeric keys.
		
		// @Hack, we check space here because we want it for a command.
		// Maybe have a pattern for shift?
		plore_key Modifier = PloreKey_None;
		if (IsAlpha(C) || IsNumeric(C) || C == ' ') {
			if (BufferedInput.sKeys[PK]) {
				Modifier = PloreKey_Shift;
			} 
		}
		
		if (BufferedInput.cKeys[PK]) {
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
						
						case VimCommandState_Cancel:
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
							vim_command_type StartType = Command.Type;
							VimCommands[VimContext->ActiveCommand.Type](State, Tab, VimContext, Tab->FileContext, VimContext->ActiveCommand);
							
							if (VimContext->ActiveCommand.Type != StartType) {
								ResetListerState(VimContext);
							} else {
								ResetVimState(VimContext);
							}
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
	
	for (u64 Split = 0; Split < State->SplitTabCount; Split++) {
		Tab = State->Tabs + State->SplitTabs[Split];
		
		//
		// NOTE(Evan): GUI stuff.
		//
		u64 Cols = 3;
		
		f32 FontHeight = State->Font->Data[State->Font->CurrentFont]->Height;
		f32 FontWidth = State->Font->Data[State->Font->CurrentFont]->Data[0].xadvance;
		
		f32 FileRowHeight = FontHeight + 4.0f;//36.0f;
		f32 FooterPad = 0;
		f32 FooterHeight = FontHeight*2 - 4.0f;//60;
		f32 fCols = (f32) Cols;
		f32 PadX = 10.0f;
		f32 PadY = 10.0f;
		v2 SplitDimensions = V2(PlatformAPI->WindowDimensions.X / State->SplitTabCount, PlatformAPI->WindowDimensions.Y);
		
		f32 StartW = (((SplitDimensions.X  - 0)            - (fCols + 1) * PadX) / fCols);
		f32 StartH = ((SplitDimensions.Y   - FooterHeight) - (1     + 1) * PadY) / 1;
		f32 W = StartW;
		f32 H = StartH;
		f32 X = Split*SplitDimensions.X + 10;
		f32 Y = 0;
		
		typedef struct plore_viewable_directory {
			plore_file_listing *File;
			b64 Focus;
		} plore_viewable_directory;
		
		plore_viewable_directory ViewDirectories[3] = {
			[0] = {
				.File = !Tab->FileContext->InTopLevelDirectory ? &Tab->DirectoryState->Parent : 0,
			},
			[1] = {
				.File = &Tab->DirectoryState->Current,
				.Focus = true,
			}, 
			[2] = {
				.File = Tab->DirectoryState->Cursor.Valid ? &Tab->DirectoryState->Cursor : 0,
			},
			
		};
		
		typedef struct image_preview_handle {
			plore_path Path;
			platform_texture_handle Texture;
			b64 Allocated;
			b64 LoadedOkay;
		} image_preview_handle;
		
		b64 StoleFocus = false;
		
		b64 ExclusiveListerMode = State->VimContext->Mode == VimMode_Lister && State->VimContext->ListerState.HideFiles;
		
		if (Window(State->VimguiContext, (vimgui_window_desc) {
					   .ID = (u64) Tab->DirectoryState->Current.File.Path.Absolute,
					   .Rect = { 
						   .P = V2(X, 0), 
						   .Span = { 
							   SplitDimensions.X, 
							   SplitDimensions.Y, 
						   } 
					   },
					   .BackgroundColour = WidgetColour_Black,
					   .NeverFocus = true,
				   })) {
			
			// NOTE(Evan): Tabs.
			f32 TabWidth = FontWidth*16;//240;
			f32 TabHeight = FontHeight*1.6f;//48;
			u64 TabCount = 0;
			
			plore_tab *Active = GetCurrentTab(State);
			if (!ExclusiveListerMode) {
				for (u64 T = 0; T < ArrayCount(State->Tabs); T++) {
					plore_tab *Tab = State->Tabs + T;
					if (Tab->Active) {
						TabCount++;
						
						widget_colour_flags BackgroundColourFlags = WidgetColourFlags_Default;
						widget_colour BackgroundColour = WidgetColour_Default;
						
						if (Tab == Active) BackgroundColourFlags = WidgetColourFlags_Focus;
						
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
										   .Alignment = VimguiLabelAlignment_CenterHorizontal,
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
							SetCurrentTab(State, T);
						}
					}
				}
			}
			
			if (State->TabCount) {
				Y += TabHeight+2*PadY;
				H -= TabHeight-PadY;
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
			// @Cleanup, this is overloaded between "global" insert and lister insert to prevent copypasta; maybe that would be preferable.
			if (VimContext->Mode == VimMode_Insert || VimContext->ListerState.Mode == VimListerMode_ISearch) {
				char *InsertPrompt = 0;
				if (VimContext->Mode == VimMode_Insert) {
					InsertPrompt = VimCommandInsertPrompts[VimContext->ActiveCommand.Type];
				} else if (VimContext->ListerState.Mode == VimListerMode_ISearch) {
					InsertPrompt = "ISearch: "; 
				}
				
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
								   .P    = V2(PadX, SplitDimensions.Y - 2*FooterHeight - 2*PadY), 
								   .Span = V2(SplitDimensions.X-2*PadX, FooterHeight + PadY)
							   },
							   .BackgroundColourFlags = WidgetColourFlags_Focus,
						   })) {
				}
			}
			
			if (VimContext->Mode == VimMode_Lister) {
				Assert(VimContext->ActiveCommand.Type);
				
				u64 ListerCount = 0;
				vim_lister_state *Lister = &VimContext->ListerState;
				
				u64 MaybeInsertHeight = 0;
				f32 ListerArea = (SplitDimensions.Y - (FooterHeight+PadY));
				if (VimContext->ListerState.Mode == VimListerMode_ISearch) {
					ListerArea -= FooterHeight+PadY;
					MaybeInsertHeight = FooterHeight+2*PadY;
				}
				
				u64 RowMax = ListerArea / FooterHeight;
				u64 ListStart = ((((Lister->Cursor)/RowMax))*RowMax) % Lister->Count;
				
				char *ListerFilter = 0;
				if (VimContext->CommandKeyCount) {
					u64 FilterSize = 256;
					ListerFilter = PushBytes(&State->FrameArena, FilterSize);
					VimKeysToString(ListerFilter, FilterSize, VimContext->CommandKeys);
				}
				
				for (u64 L = ListStart; L < Lister->Count; L++) {
					if (ListerCount >= RowMax) break;
					char *Title = Lister->Titles[L];
					char *Secondary = Lister->Secondaries[L];
					if (!Secondary) Secondary = "";
					
					if (Title) {
						u64 ID = (u64) Title;
						ListerCount++;
						
						// NOTE(Evan): We left-justify here because there's VIMGUI has no alignment option to allow for it!
						u64 ListerBufferSize = 512;
						char *ListerBuffer = PushBytes(&State->FrameArena, ListerBufferSize);
						StringPrintSized(ListerBuffer, ListerBufferSize, "%-30s", Title);
						
						text_colour TextColour = TextColour_Default;
						if (ListerFilter) {
							if (SubstringNoCase(Title, ListerFilter).IsContained) TextColour = TextColour_PromptCursor;
						}
						
						if (Button(State->VimguiContext, (vimgui_button_desc) {
									   .ID = ID,
									   .Title = {
										   .Text = ListerBuffer,
										   .Pad = V2(16, 8),
										   .Alignment = VimguiLabelAlignment_Left,
										   .Colour = TextColour,
									   },
									   .Secondary = {
										   .Text = Secondary,
										   .Pad = V2(16, 8),
										   .Alignment = VimguiLabelAlignment_Left,
										   .Colour = TextColour_PromptCursor,
									   },
									   .Rect = {
										   .P = V2(PadX, SplitDimensions.Y - MaybeInsertHeight - FooterHeight - (RowMax-ListerCount+1)*FooterHeight - PadY),
										   .Span = V2(SplitDimensions.X-2*PadX, FooterHeight)
									   },
									   .BackgroundColour = (L == VimContext->ListerState.Cursor) ? WidgetColour_Secondary : WidgetColour_Primary,
									   .BackgroundColourFlags = WidgetColourFlags_Focus,
									   })) {
							DrawText("I do nothing!");
						}
						
						H -= FooterHeight;
					}
				}
				
				H -= 20;
				
				StoleFocus = true;
			}
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
						
						if (Candidate->Shell) {
							StringPrintSized(CandidateString, CandidateSize, "%s, arg: %s",
											 VimCommandDescriptions[Candidate->Type],
											 Candidate->Shell
											 );
						} else {
							StringPrintSized(CandidateString, CandidateSize, "%s", VimCommandDescriptions[Candidate->Type]);
						}
						
						u64 ID = Candidate->Shell ? (u64) Candidate->Shell : (u64) CandidateString + Candidate->Type;
						
						if (Button(State->VimguiContext, (vimgui_button_desc) {
									   .ID = ID,
									   .Title = {
										   .Text = BindingString,
										   .Pad = V2(16, 16),
										   .Alignment = VimguiLabelAlignment_Left,
									   },
									   .Secondary = {
										   .Text = CandidateString,
										   .Pad = V2(32, 16),
										   .Alignment = VimguiLabelAlignment_Left,
										   .Colour = TextColour_PromptCursor,
									   },
									   .Rect = {
										   .P = V2(PadX, SplitDimensions.Y - FooterHeight - CandidateCount*(FooterHeight + PadY) + PadY - 20),
										   .Span = V2(SplitDimensions.X-2*PadX, FooterHeight + PadY)
									   },
									   .BackgroundColour =  WidgetColour_Primary,
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
			
			u64 CursorInfoBufferSize = 512;
			char *CursorInfoBuffer = PushBytes(&State->FrameArena, CursorInfoBufferSize);
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
				StringPrintSized(CursorInfoBuffer, 
								 CursorInfoBufferSize,
							     "[%s] %s %s", 
							     (CursorFile->Type == PloreFileNode_Directory) ? "directory" : "file", 
								 CursorFile->Path.FilePart,
								 PloreTimeFormat(&State->FrameArena, CursorFile->LastModification, "%a %b %d/%m/%y")
								 );
			} else {
				StringPrintSized(CursorInfoBuffer, 
								 CursorInfoBufferSize,
							     "[no selection]"
								 );
			}
			
			if (Button(State->VimguiContext, (vimgui_button_desc) {
					   .Title     = { 
						   .Text = CursorInfoBuffer, 
						   .Alignment = VimguiLabelAlignment_Left, 
						   .Colour = TextColour_CursorInfo,
						   .Pad = V2(8, 8),
					   },
					   .Secondary = { 
						   .Text = Buffer,
						   .Alignment = VimguiLabelAlignment_Left,
						   .Pad = V2(0, 8),
						   .Colour = TextColour_Prompt
					   },
					   .Rect = { 
						   .P    = V2(0, SplitDimensions.Y - FooterHeight + FooterPad), 
						   .Span = V2(SplitDimensions.X-2*PadX, FooterHeight + PadY-20)
					   },
						   })) {
				DoShowCommandList(State, GetCurrentTab(State), State->VimContext, Tab->FileContext, (vim_command) {
									  .State = VimCommandState_Start,
									  .Type = VimCommandType_ShowCommandList,
								  });
			}
			
			
			if (!ExclusiveListerMode) {
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
								   .BackgroundColour      = WidgetColour_Window,
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
										if (!PassesFilter) TextColour = TextColour_PrimaryFade;
									} else {
										TextColour = TextColour_Secondary;								
										if (!PassesFilter) TextColour = TextColour_SecondaryFade;
									}
									
									
									b64 DisplayFileNumbers = false;
									u64 FileNameSize = 256;
									char *FileName = PushBytes(&State->FrameArena, FileNameSize);
									if (DisplayFileNumbers) {
										StringPrintSized(FileName, FileNameSize, "%-3d %s", Row, Listing->Entries[Row].Path.FilePart);
									} else {
										StringPrintSized(FileName, FileNameSize, "%s", Listing->Entries[Row].Path.FilePart);
										
									}
									
									char *Timestamp = PloreTimeFormat(&State->FrameArena, RowEntry->LastModification, "%b %d/%m/%y");
									char *SecondaryText = "";
									if (!Tab->FilterState->HideFileMetadata) {
										SecondaryText = Timestamp;
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
								
								// TODO(Evan): Make file loading asynchronous!
								// TODO(Evan): Make preview work for other image types.
								switch (Listing->File.Extension) {
									case PloreFileExtension_BMP:
									case PloreFileExtension_PNG:
									case PloreFileExtension_JPG: {
										for (u64 I = 0; I < ArrayCount(ImagePreviewHandles); I++) {
											image_preview_handle *Handle = ImagePreviewHandles + I;
											if (StringsAreEqual(Handle->Path.Absolute, Listing->File.Path.Absolute)) {
												MyHandle = Handle;
												break;
											}
										}
										if (!MyHandle) {
											MyHandle = ImagePreviewHandles + ImagePreviewHandleCursor;
											ImagePreviewHandleCursor = (ImagePreviewHandleCursor + 1) % ArrayCount(ImagePreviewHandles);
											
											// NOTE(Evan): Evict old handles if we need room.
											if (MyHandle->Allocated) {
												Platform->DestroyTextureHandle(MyHandle->Texture);
												MyHandle->Texture = ClearStruct(platform_texture_handle);
											}
											MyHandle->Path = Listing->File.Path;
											
											load_image_result ImageResult = LoadImage(&State->FrameArena, Listing->File.Path.Absolute, 1024, 1024);
											
											if (ImageResult.LoadedSuccessfully) MyHandle->Texture = ImageResult.Texture;
										}
										
										Assert(MyHandle);
										
										if (MyHandle->Texture.Opaque) { 
											Image(State->VimguiContext, (vimgui_image_desc) { 
														  .ID = (u64) MyHandle,
														  .Texture = MyHandle->Texture,
														  .Rect = {
															  .Span = V2(512, 512),
														  }, 
														  .Centered = true, // @Cleanup
												  });
										} else {
											Button(State->VimguiContext, (vimgui_button_desc) {
														   .Rect = {
															   .P = V2(FontHeight, H/2),
															   .Span = V2(W-2*FontHeight, FontHeight*4),
														   },
														   .Title = {
															   .Text = "Could not load image.",
															   .Alignment = VimguiLabelAlignment_Center,
														   },
												   });
										}
									} break;
									
									// TODO(Evan): Easier way to specify inclusion/exclusion of extensions.
									default: {
										platform_readable_file TheFile = Platform->DebugOpenFile(Listing->File.Path.Absolute);
										if (TheFile.OpenedSuccessfully) {
											u64 TextBoxSize = Kilobytes(8);
											char *Text = PushBytes(&State->FrameArena, TextBoxSize);
											
											platform_read_file_result ReadResult = Platform->DebugReadFileSize(TheFile, Text, TextBoxSize);
											Assert(ReadResult.ReadSuccessfully);
											
											u64 TextBoxID = (u64)HashString(Listing->File.Path.Absolute);
											
											TextBox(State->VimguiContext, (vimgui_text_box_desc) {
														.ID = TextBoxID,
														.Text = Text,
														.Rect = {
															.P = V2(PadX, FontHeight+4),
															.Span = V2(W-2*PadX, H-FooterHeight-PadY),
														},
											});
											
											Platform->DebugCloseFile(TheFile);
										}
									} break;
								}
								
							} break;
							
							InvalidDefaultCase;
						}
						WindowEnd(State->VimguiContext);
					}
					
					X += W + PadX;
				}
			}
			
			WindowEnd(State->VimguiContext);
		}
	}
	
	
	
	VimguiEnd(State->VimguiContext);
	
#if defined(PLORE_INTERNAL)
	FlushText();
#endif
	
	return((plore_frame_result) { 
			   .RenderList = State->VimguiContext->RenderList,
			   .ShouldQuit = State->ShouldQuit,
		   });
}

plore_inline b64
IsYanked(plore_file_context *Context, plore_path *Yankee) {
	for (u64 Y = 0; Y < Context->YankedCount; Y++) {
		plore_path *It = Context->Yanked + Y;
		if (StringsAreEqual(It->Absolute, Yankee->Absolute)) return(true);
	}
	
	return(false);
}

plore_inline b64
IsSelected(plore_file_context *Context, plore_path *Selectee) {
	for (u64 S = 0; S < Context->SelectedCount; S++) {
		plore_path *It = Context->Selected + S;
		if (StringsAreEqual(It->Absolute, Selectee->Absolute)) return(true);
	}
	
	return(false);
}

internal void
ToggleHelper(plore_file_context *Context, plore_path *Togglee, plore_path *List, u64 ListMax, u64 *ListCount) {
	// NOTE(Evan): Toggle and exit if its in the list.
	for (u64 S = 0; S < *ListCount; S++) {
		plore_path *It = List + S;
		if (StringsAreEqual(It->Absolute, Togglee->Absolute)) {
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
	StringCopy(Desc.Path.Absolute, Listing->File.Path.Absolute, PLORE_MAX_PATH);
	StringCopy(Desc.Path.FilePart, Listing->File.Path.FilePart, PLORE_MAX_PATH);
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
		Listing->File.Bytes = Desc.Bytes;
		Listing->File.LastModification = Desc.LastModification;
		Listing->File.Extension = GetFileExtension(Desc.Path.FilePart).Extension; 
		Listing->Valid = true;
	}
	
}


plore_inline b64 
PloreFileSortHelper(plore_file *A, plore_file *B, file_sort_mask SortMask, b64 Ascending) {
	b64 Result = false;
	
	switch (SortMask) {
		case FileSort_Name: {
			Result = StringCompare(A->Path.FilePart, B->Path.FilePart);
			if (Ascending) Result = !Result;
		} break;
		
		// NOTE(Evan): In this mode, files always go before extensions, while respecting ascending/descending order by extension.
		// Directories will always be sorted descending by name.
		case FileSort_Extension: {
			if (A->Type == PloreFileNode_Directory && B->Type == PloreFileNode_File) {
				Result = true;
			} else if (A->Type == PloreFileNode_File && B->Type == PloreFileNode_Directory) {
				Result = false;
			} else {
				if (A->Type == PloreFileNode_File && B->Type == PloreFileNode_File) {
					Result = StringCompareReverse(A->Path.FilePart, B->Path.FilePart);
					if (Ascending) Result = !Result;
				} else {
					Result = StringCompare(A->Path.FilePart, B->Path.FilePart);
				}
				
			}
		} break;
		
		case FileSort_Modified: {
			Result = A->LastModification.T > B->LastModification.T;
			if (Ascending) Result = !Result;
		} break;
		
		case FileSort_Size: {
			Result = A->Bytes > B->Bytes;
			if (Ascending) Result = !Result;
		} break;
	}
	
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
			
			if (SourceLength) StringCopy(Tail+1, Tail, DestLength);
			else *Tail = '\0';
		} else {
			Current++;
		}
		
	}
	
	return(S);
}


internal b64
StringMatchesFilter(char *S, char *Pattern, text_filter *Filter) {
	b64 Result = SubstringNoCase(S, Pattern).IsContained;
	if (Filter->State == TextFilterState_Show) Result = !Result;
	return(Result);
}

internal void
FilterFileListing(memory_arena *Arena, plore_tab *Tab, plore_file_listing *Listing) {
	text_filter *TabFilter = 0;
	char TabTextFilterPattern[PLORE_MAX_PATH] = {0};
	if (Tab->FilterState->GlobalListingFilter.TextCount) {
		TabFilter = &Tab->FilterState->GlobalListingFilter;
		StringCopy(TabFilter->Text, TabTextFilterPattern, ArrayCount(TabTextFilterPattern));
		RemoveSeparators(TabTextFilterPattern, ArrayCount(TabTextFilterPattern));
	}
	
	text_filter *DirectoryFilter = 0;
	char DirectoryTextFilterPattern[PLORE_MAX_PATH] = {0};
	plore_file_listing_info *Info = GetOrCreateFileInfo(Tab->FileContext, &Listing->File.Path).Info;
	
	if (Info->Filter.TextCount) {
		DirectoryFilter = &Info->Filter;
		StringCopy(DirectoryFilter->Text, DirectoryTextFilterPattern, ArrayCount(DirectoryTextFilterPattern));
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
			StringCopy(File->Path.FilePart, FileNamePattern, ArrayCount(FileNamePattern));
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
		StringCopy(FilePart, CurrentCopy.File.Path.FilePart, ArrayCount(CurrentCopy.File.Path.FilePart));
		
		if (Platform->IsPathTopLevel(CurrentCopy.File.Path.Absolute, PLORE_MAX_PATH)) {
			StringCopy(CurrentCopy.File.Path.Absolute, CurrentCopy.File.Path.FilePart, PLORE_MAX_PATH);
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
				
				if (StringsAreEqual(File->Path.Absolute, CurrentState->Current.File.Path.Absolute)) {
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
		Count += StringCopy(PloreKeyStrings[Keys[K].Input], Buffer, Size - Count);
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
		Result.ErrorReason = (char *)stbi_failure_reason();
	}
	
	
	return(Result);
}
