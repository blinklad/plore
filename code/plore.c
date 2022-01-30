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

typedef enum file_metadata_display {
	FileMetadataDisplay_None,
	FileMetadataDisplay_Basic,
	FileMetadataDisplay_Extended,
	_FileMetadataMask_ForceU64 = 0xffffffffull,
} file_metadata_display;

typedef struct plore_file_filter_state {
	// NOTE(Evan): This is used to temporarily match against files during ISearch, including files processed by TextFilter.
	text_filter ISearchFilter;

	// NOTE(Evan): This hard-removes matching/non-matching file entries from the directory listing, persisting for the tab's lifetime.
	// Matching/non-matching is given by the flag below.
	text_filter GlobalListingFilter;

	plore_path_buffer ExtensionFilter;
	u64 ExtensionFilterCount;

	file_sort_mask SortMask;
	b64 SortAscending;

	struct {
		plore_file_extension Extensions[PloreFileExtension_Count];
		plore_file_node FileNodes[PloreFileNode_Count];
		b64 HiddenFiles;
	} HideMask;

	file_metadata_display MetadataDisplay;

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

	// NOTE(Evan): Each tab has their own file context (selection, cursor state), but can change the global yank state.
	plore_tab Tabs[PloreTab_Count];
	u64 TabCount;
	u64 TabCurrent;

	// NOTE(Evan): Globally yanked files, so files can be pasted/moved between tabs.
	plore_map Yanked;

	// Updated asynchronously with the metadata corresponding to a recursive traversal of the focused directories' cursor entry.
	plore_directory_query_state DirectoryQuery;

	plore_vim_context *VimContext;
	plore_vimgui_context *VimguiContext; // TODO(Evan): Move this so it's a per-tab thing.
	plore_render_list *RenderList;

	memory_arena Arena;      // NOTE(Evan): Never freed.
	memory_arena FrameArena; // NOTE(Evan): Freed at the beginning of each frame.

	plore_font *Font;
	platform_taskmaster *Taskmaster;
} plore_state;


internal b64
SetCurrentTab(plore_state *State, u64 NewTab);

// NOTE(Evan): Called by SetCurrentTab internally.
internal void
TabInit(plore_state *State, plore_tab *Tab);

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
GetImpliedSelectionPath(plore_state *State);

internal f32
GetFontHeight(plore_font *Font, u64 ID);

internal f32
GetFontWidth(plore_font *Font, u64 ID);

internal f32
GetCurrentFontHeight(plore_font *Font);

internal f32
GetCurrentFontWidth(plore_font *Font);

internal void
ToggleSelected(plore_map *Map, plore_path *File);

internal void
ToggleYanked(plore_map *Map, plore_path *File);

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

#define PLORE_MAP_IMPLEMENTATION
#include "plore_map.h"


#include "plore_table.c"
#include "plore_task.c"
#include "plore_vim.c"
#include "plore_vimgui.c"
#include "plore_time.c"
#include "plore_tab.c"

internal plore_directory_query_state
GetDirectoryInfo(plore_state *State, plore_path *Path) {
	plore_directory_query_state Result = {0};

	if (StringsAreEqual(State->DirectoryQuery.Path.Absolute, Path->Absolute)) {
		Result = State->DirectoryQuery;
	} else {
		Platform->DirectorySizeTaskBegin(Platform->Taskmaster, Path, &State->DirectoryQuery);
	}

	return(Result);
}

internal plore_file *
GetCursorFile(plore_state *State) {
	plore_tab *Tab = GetCurrentTab(State);
	plore_file_listing_info_get_or_create_result CursorResult = GetOrCreateFileInfo(Tab->FileContext, &Tab->DirectoryState->Current.File.Path);
	plore_file *CursorEntry = Tab->DirectoryState->Current.Entries + CursorResult.Info->Cursor;

	return(CursorEntry);
}

internal plore_path *
GetImpliedSelectionPath(plore_state *State) {
	plore_path *Result = 0;

	plore_tab *Tab = GetCurrentTab(State);
	plore_file_context *FileContext = Tab->FileContext;

	if (FileContext->Selected.Count == 1) {
		Result = MapIter(&FileContext->Selected).Key;
	} else if (!FileContext->Selected.Count) {
		plore_file_listing *Cursor = &Tab->DirectoryState->Cursor;
		if (Cursor->Valid) {
			GetOrCreateFileInfo(FileContext, &Cursor->File.Path);
			Result = &Cursor->File.Path;
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


//
// NOTE(Evan): Font metric helpers.
//
internal f32
GetFontHeight(plore_font *Font, u64 ID) {
	Assert(ID < PloreBakedFont_Count);
	f32 Result = Font->Data[ID]->Height;
	return(Result);
}

internal f32
GetFontWidth(plore_font *Font, u64 ID) {
	Assert(ID < PloreBakedFont_Count);
	f32 Result = Font->Data[ID]->Data[0].xadvance;
	return(Result);
}

internal f32
GetCurrentFontHeight(plore_font *Font) {
	f32 Result = Font->Data[Font->CurrentFont]->Height;
	return(Result);
}

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

internal plore_state *
PloreInit(memory_arena *Arena) {
	plore_state *State = PushStruct(Arena, plore_state);

	State->Initialized = true;
	State->FrameArena = SubArena(Arena, Megabytes(128), 16);
	STBIFrameArena = &State->FrameArena;

	// MAINTENANCE(Evan): Don't copy the main arena by value until we've allocated the frame arena.
	State->Arena = *Arena;

	State->Yanked = MapInit(&State->Arena, plore_path, void, 256);
	for (u64 T = 0; T < ArrayCount(State->Tabs); T++) {
		plore_tab *Tab = State->Tabs + T;
		Tab->Arena = SubArena(&State->Arena, Megabytes(2), 16);
	}

	TabInit(State, &State->Tabs[0]);
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

	State->Taskmaster = Platform->Taskmaster;

	return(State);
}

PLORE_DO_ONE_FRAME(PloreDoOneFrame) {
	plore_state *State = (plore_state *)PloreMemory->PermanentStorage.Memory;

	if (!State->Initialized) {
		PlatformInit(PlatformAPI);
		State = PloreInit(&PloreMemory->PermanentStorage);

		SynchronizeCurrentDirectory(&State->FrameArena, GetCurrentTab(State));

	} else {
		ClearArena(&State->FrameArena);
	}

#if defined(PLORE_INTERNAL)
	if (PloreInput->DLLWasReloaded) {
		PlatformInit(PlatformAPI);
		STBIFrameArena = &State->FrameArena;
		State->Taskmaster = Platform->Taskmaster;
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

	// NOTE(Evan): We Begin() here as the render list is shared for debug draw purposes.
	VimguiBegin(State->VimguiContext, BufferedInput, PlatformAPI->WindowDimensions);

	b64 DidInput = false;
	vim_key BufferedKeys[64] = {0};
	u64 BufferedKeyCount = 0;

	// @Hack, fix this up once we've implemented F-keys as well.
	if (VimContext->CommandKeyCount < ArrayCount(VimContext->CommandKeys)) {
		if (BufferedInput.pKeys[PloreKey_Escape]) {
			VimContext->CommandKeys[VimContext->CommandKeyCount++] = (vim_key) {
				.Input = PloreKey_Escape,
			};
			DidInput = true;
		} else if (BufferedInput.pKeys[PloreKey_Backspace]) {
			VimContext->CommandKeys[VimContext->CommandKeyCount++] = (vim_key) {
				.Input = PloreKey_Backspace,
			};
			DidInput = true;
		}
	}

#if 0
	for (u64 K = 0; K < BufferedInput._InputCount; K++) {
		if (VimContext->CommandKeyCount == ArrayCount(VimContext->CommandKeys)) break;
		plore_key_input *Input = BufferedInput._Input + K;

		VimContext->CommandKeys[VimContext->CommandKeyCount++] = (vim_key) {
			.Input = Input->Key,
			.Modifier = Input->Modifier,
			.Pattern = IsNumeric(Input->Key) ? VimPattern_Digit : VimPattern_None,
		};

		DidInput = true;

	}
#endif
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


	if (DidInput) {
		int BreakHere = 5;
		BreakHere;
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

	Tab = GetCurrentTab(State);

	//
	// NOTE(Evan): GUI stuff.
	//
	u64 Cols = 3;

	f32 FontHeight = State->Font->Data[State->Font->CurrentFont]->Height;
	f32 FontWidth = State->Font->Data[State->Font->CurrentFont]->Data[0].xadvance;

	f32 FileRowHeight = FontHeight + 4.0f;//36.0f;
	f32 CursorFileRowHeight = FileRowHeight * 2.0f;
	f32 FooterPad = 0;
	f32 FooterHeight = FontHeight*2 - 4.0f;//60;
	f32 fCols = (f32) Cols;
	f32 PadX = 10.0f;
	f32 PadY = 10.0f;
	v2 WindowDimensions = PlatformAPI->WindowDimensions;

	f32 StartW = (((WindowDimensions.X  - 0)            - (fCols + 1) * PadX) / fCols);
	f32 StartH = ((WindowDimensions.Y   - FooterHeight) - (1     + 1) * PadY) / 1;
	f32 W = StartW;
	f32 H = StartH;
	f32 X = PadX;
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
						   WindowDimensions.X,
						   WindowDimensions.Y,
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

					temp_string Number = TempString(&State->FrameArena, 32);
					TempPrint(Number, "%d", T+1);

					if (Button(State->VimguiContext, (vimgui_button_desc) {
								   .ID = (u64) Tab,
								   .Rect = {
									   .P =    { (TabCount-1)*(TabWidth+PadX), PadY },
									   .Span = { TabWidth, TabHeight },
								   },
								   .Title = {
									   .Text = Tab->CurrentDirectory.FilePart,
									   .Colour = (Tab == Active) ? TextColour_TabActive : TextColour_Tab,
									   .Pad = V2(0, 6),
									   .Alignment = VimguiLabelAlignment_CenterHorizontal,
								   },
								   .Secondary = {
									   .Text = Number.Buffer,
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

			u64 Size = 128;
			temp_string Insert = TempString(&State->FrameArena, Size);
			TempCat(Insert, "%s ", InsertPrompt);
			TempCat(Insert, "%s", VimKeysToString(PushBytes(&State->FrameArena, Size), Size, VimContext->CommandKeys).Buffer);

			H -= (FooterHeight + 2*PadY);

			StoleFocus = true;

			Button(State->VimguiContext, (vimgui_button_desc) {
						   .ID = (u64) Insert.Buffer,
						   .Title = {
							   .Text = Insert.Buffer,
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
							   .P    = V2(0, WindowDimensions.Y - 2*FooterHeight - 2*PadY),
							   .Span = V2(WindowDimensions.X-2*PadX, FooterHeight + PadY)
						   },
						   .BackgroundColourFlags = WidgetColourFlags_Focus,
		    });
		}

		if (VimContext->Mode == VimMode_Lister) {
			Assert(VimContext->ActiveCommand.Type);

			u64 ListerCount = 0;
			vim_lister_state *Lister = &VimContext->ListerState;

			u64 MaybeInsertHeight = 0;
			f32 ListerArea = (WindowDimensions.Y - (FooterHeight+PadY));
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
					temp_string Lister = TempString(&State->FrameArena, 512);
					TempPrint(Lister, "%-30s", Title);

					text_colour TextColour = TextColour_Default;
					if (ListerFilter) {
						if (SubstringNoCase(Title, ListerFilter).IsContained) TextColour = TextColour_PromptCursor;
					}


					// FIXME(Evan): Lister movement is backwards if drawn top-to-bottom.
					v2 RowP = V2(0, WindowDimensions.Y - MaybeInsertHeight - FooterHeight - (RowMax-ListerCount+1)*FooterHeight - PadY);
					if (!ExclusiveListerMode) {
						RowP.Y = WindowDimensions.Y - FooterHeight - ListerCount*FooterHeight;
					}

					if (Button(State->VimguiContext, (vimgui_button_desc) {
								   .ID = ID,
								   .Title = {
									   .Text = Lister.Buffer,
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
									   .P = RowP,
									   .Span = V2(WindowDimensions.X-2*PadX, FooterHeight)
								   },
								   .BackgroundColour = (L == VimContext->ListerState.Cursor) ? WidgetColour_Secondary : WidgetColour_Primary,
								   .BackgroundColourFlags = WidgetColourFlags_Focus,
								   })) {
						ListerEnd(State, L);
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

					temp_string CandidateString = TempString(&State->FrameArena, 256);

					if (Candidate->Shell) {
						TempPrint(CandidateString, "%s, arg: %s",
								  VimCommandDescriptions[Candidate->Type],
								  Candidate->Shell
								  );
					} else {
						TempPrint(CandidateString, "%s", VimCommandDescriptions[Candidate->Type]);
					}

					u64 ID = Candidate->Shell ? (u64) Candidate->Shell : (u64) CandidateString.Buffer + Candidate->Type;

					if (Button(State->VimguiContext, (vimgui_button_desc) {
								   .ID = ID,
								   .Title = {
									   .Text = BindingString,
									   .Pad = V2(16, 16),
									   .Alignment = VimguiLabelAlignment_Left,
								   },
								   .Secondary = {
									   .Text = CandidateString.Buffer,
									   .Pad = V2(32, 16),
									   .Alignment = VimguiLabelAlignment_Left,
									   .Colour = TextColour_PromptCursor,
								   },
								   .Rect = {
									   .P = V2(0, WindowDimensions.Y - FooterHeight - CandidateCount*(FooterHeight + PadY) + PadY - 20),
									   .Span = V2(WindowDimensions.X-2*PadX, FooterHeight + PadY)
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

		temp_string CursorInfoString = TempString(&State->FrameArena, 512);
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

			if (CursorFile->Type == PloreFileNode_File) {
				get_size_and_label_result SizeAndLabel = GetSizeAndLabel(CursorFile->Bytes);
				TempPrint(CursorInfoString,
						  "[file] %s - %d%s - %s",
						  CursorFile->Path.FilePart,
						  SizeAndLabel.Size,
						  SizeAndLabel.Label,
						  PloreTimeFormat(&State->FrameArena, CursorFile->LastModification, "%a %b %d/%m/%y")
						  );
			} else {
				plore_directory_query_state DirectoryQuery = GetDirectoryInfo(State, &CursorFile->Path);
				get_size_and_label_result SizeAndLabel = GetSizeAndLabel(DirectoryQuery.SizeProgress);
				TempPrint(CursorInfoString,
						  "[dir] %s - %d%s %d file/s %d dir/s - %s",
						  CursorFile->Path.FilePart,
						  SizeAndLabel.Size,
						  SizeAndLabel.Label,
						  DirectoryQuery.FileCount,
						  DirectoryQuery.DirectoryCount,
						  PloreTimeFormat(&State->FrameArena, CursorFile->LastModification, "%a %b %d/%m/%y")
						  );
			}
		} else {
			TempPrint(CursorInfoString,
				      "[no selection]"
					  );
		}

		if (Button(State->VimguiContext, (vimgui_button_desc) {
				   .Title     = {
					   .Text = CursorInfoString.Buffer,
					   .Alignment = VimguiLabelAlignment_Left,
					   .Colour = TextColour_CursorInfo,
					   .Pad = V2(8, FontHeight/4),
				   },
				   .Secondary = {
					   .Text = Buffer,
					   .Alignment = VimguiLabelAlignment_Left,
					   .Pad = V2(0, FontHeight/4),
					   .Colour = TextColour_Prompt
				   },
				   .Rect = {
					   .P    = V2(0, WindowDimensions.Y - FooterHeight + FooterPad),
					   .Span = V2(WindowDimensions.X-2*PadX, FooterHeight + PadY-20)
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
				plore_file_listing_info *RowCursor = MapGet(&Tab->FileContext->FileInfo, &Listing->File.Path).Value;
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
							   .ID = (u64)Title+(u64)Tab,
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

								b64 WeAreOnCursor = RowCursor && RowCursor->Cursor == Row;
								if (WeAreOnCursor) WidgetColourFlags = WidgetColourFlags_Focus;

								if (MapGet(&State->Yanked, &RowEntry->Path).Exists) {
									BackgroundColour = WidgetColour_RowSecondary;
								} else if (MapGet(&Tab->FileContext->Selected, &RowEntry->Path).Exists) {
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
								temp_string FileName = TempString(&State->FrameArena, 256);
								if (DisplayFileNumbers) {
									TempPrint(FileName, "%-3d %s", Row, Listing->Entries[Row].Path.FilePart);
								} else {
									TempPrint(FileName, "%s", Listing->Entries[Row].Path.FilePart);

								}

								// TODO(Evan): Extended file metadata!
								temp_string SecondaryText = TempString(&State->FrameArena, 256);

								temp_string BodyText = {0};

								if (Tab->FilterState->MetadataDisplay) {
									u64 EntrySize = RowEntry->Bytes;
									if (RowEntry->Type == PloreFileNode_File || (Directory->Focus && WeAreOnCursor)) {
										local f32 WiggleAccum = 0;
										char *Wiggler = " ";
										b64 DoWiggle = false;


										char *EntrySizeLabel = " b";

										plore_directory_query_state DirectoryQuery = {0};
										if (RowEntry->Type == PloreFileNode_Directory && Directory->Focus) {
											DirectoryQuery = GetDirectoryInfo(State, &RowEntry->Path);
											EntrySize = DirectoryQuery.SizeProgress;
											if (!DirectoryQuery.Completed) {
												DoWiggle = true;
											}

											if (DoWiggle) {
												WiggleAccum += State->DT;
												if (WiggleAccum < 0.25f) {
													Wiggler = "|";
												} else if (WiggleAccum < 0.5f) {
													Wiggler = "/";
												} else if (WiggleAccum < 0.75f) {
													Wiggler = "-";
												} else if (WiggleAccum <= 1.0f) {
													Wiggler = "\\";
												} else {
													WiggleAccum = 0.0f;
													Wiggler = "|";
												}
											} else {
												Wiggler = "-";
											}

											BodyText = TempString(&State->FrameArena, 256);
											TempPrint(BodyText,
													  "%d file/s, %d dir/s",
													  DirectoryQuery.FileCount,
													  DirectoryQuery.DirectoryCount);
										}

										get_size_and_label_result SizeAndLabel = GetSizeAndLabel(EntrySize);

										b64 DoDirectoryWiggle = DoWiggle && RowEntry->Type == PloreFileNode_Directory && Directory->Focus;

										TempPrint(SecondaryText,
												  "%s%4d%s %s",
												  PloreTimeFormat(&State->FrameArena, RowEntry->LastModification, "%b %d/%m/%y"),
												  SizeAndLabel.Size,
												  SizeAndLabel.Label,
												  Wiggler);


									} else {
										TempPrint(SecondaryText, "%s%8s", PloreTimeFormat(&State->FrameArena, RowEntry->LastModification, "%b %d/%m/%y"), "-");
									}

								}

								// NOTE(Evan): If we are in extended file info mode, the active directories' cursor should be larger
								// to accommodate extra space in the future(tm).
								b64 BigCursorButton = WeAreOnCursor && Directory->Focus;
								if (Button(State->VimguiContext, (vimgui_button_desc) {
											   .Title     = {
												   .Text = FileName.Buffer,
												   .Alignment = VimguiLabelAlignment_Left,
												   .Colour = TextColour,
												   .Pad = V2(4, (BigCursorButton ? 4 : 0)),
												   .ColourFlags = TextColourFlags,
											   },
											   .Secondary = {
												   .Text = SecondaryText.Buffer,
												   .Alignment = VimguiLabelAlignment_Right,
												   .Pad = V2(4, (BigCursorButton ? 4 : 0)),
												   .Colour = TextColour_Tertiary,
												   .ColourFlags = TextColourFlags,
											   },
											   .Body = {
												   .Text = BodyText.Buffer,
												   .Colour = TextColour_Tertiary,
												   .ColourFlags = TextColourFlags,
												   .Alignment = VimguiLabelAlignment_Right,
												   .Pad = V2(4, CursorFileRowHeight/2),
											   },
											   .FillWidth = true,
											   .BackgroundColour = BackgroundColour,
											   .BackgroundColourFlags = WidgetColourFlags,
											   .Rect = {
												   .Span =
												   {
													   .H = BigCursorButton ? CursorFileRowHeight : FileRowHeight,
												   }
											   },
											   .Pad = V2(0, 4),

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
										temp_string Text = TempString(&State->FrameArena, Kilobytes(8));
										platform_read_file_result ReadResult = Platform->DebugReadFileSize(TheFile, Text.Buffer, Text.Capacity);
										Text.Buffer[ReadResult.BytesRead] = '\0';
										Assert(ReadResult.ReadSuccessfully);

										u64 TextBoxID = (u64)HashString(Listing->File.Path.Absolute);

										TextBox(State->VimguiContext, (vimgui_text_box_desc) {
													.ID = TextBoxID,
													.Text = Text.Buffer,
													.Rect = {
														.P = V2(2*PadX, FontHeight+4),
														.Span = V2(W-4*PadX, H-FooterHeight-2*PadY),
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

	VimguiEnd(State->VimguiContext);

#if defined(PLORE_INTERNAL)
	FlushText();
#endif

	return((plore_frame_result) {
			   .RenderList = State->VimguiContext->RenderList,
			   .ShouldQuit = State->ShouldQuit,
		   });
}

internal void
ToggleFileStatus(plore_map *Map, plore_path *File) {
	plore_map_get_result Result = MapGet(Map, File);
	if (Result.Exists) MapRemove(Map, File);
	else               SetInsert(Map, File, 0);
}

internal void
ToggleSelected(plore_map *Map, plore_path *File) {
	ToggleFileStatus(Map, File);
}

internal void
ToggleYanked(plore_map *Map, plore_path *File) {
	ToggleFileStatus(Map, File);
}

// @Cleanup
internal void
CreateFileListing(plore_file_listing *Listing, plore_file_listing_desc Desc) {
	Assert(Listing);

	Listing->File.Type = Desc.Type;
	Listing->File.LastModification = Desc.LastModification;
	PathCopy(Desc.Path.Absolute, Listing->File.Path.Absolute);
	PathCopy(Desc.Path.FilePart, Listing->File.Path.FilePart);
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
	for (plore_file_node F = 0; F < PloreFileNode_Count; F++) if (List[F] && (FileNode == F)) return(true);
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
	plore_path_buffer TabTextFilterPattern = {0};
	if (Tab->FilterState->GlobalListingFilter.TextCount) {
		TabFilter = &Tab->FilterState->GlobalListingFilter;
		StringCopy(TabFilter->Text, TabTextFilterPattern, ArrayCount(TabTextFilterPattern));
		RemoveSeparators(TabTextFilterPattern, ArrayCount(TabTextFilterPattern));
	}

	text_filter *DirectoryFilter = 0;
	plore_path_buffer DirectoryTextFilterPattern = {0};
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
			plore_path_buffer FileNamePattern = {0};
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

	*CurrentState = ClearStruct(plore_current_directory_state);

#define PloreSortPredicate(A, B) PloreFileSortHelper(&A, &B, Tab->FilterState->SortMask, Tab->FilterState->SortAscending)

	CreateFileListing(&CurrentState->Current, ListingFromDirectoryPath(&Tab->CurrentDirectory));
	FileContext->InTopLevelDirectory = Platform->PathIsTopLevel(CurrentState->Current.File.Path.Absolute);
	FilterFileListing(FrameArena, Tab, &CurrentState->Current);

	if (CurrentState->Current.Count) PloreSort(CurrentState->Current.Entries, CurrentState->Current.Count, plore_file)

	if (CurrentState->Current.Valid && CurrentState->Current.Count) {
		plore_file_listing_info_get_or_create_result CursorResult = GetOrCreateFileInfo(FileContext, &CurrentState->Current.File.Path);


		plore_file *CursorEntry = CurrentState->Current.Entries + CursorResult.Info->Cursor;


		CreateFileListing(&CurrentState->Cursor, ListingFromFile(CursorEntry));
		FilterFileListing(FrameArena, Tab, &CurrentState->Cursor);
		if (CurrentState->Cursor.Count) PloreSort(CurrentState->Cursor.Entries, CurrentState->Cursor.Count, plore_file);
	}

	if (!FileContext->InTopLevelDirectory) {
		// CLEANUP(Evan): Doesn't need to copy twice!
		plore_file_listing CurrentCopy = CurrentState->Current;
		char *FilePart = Platform->PathPop(CurrentCopy.File.Path.Absolute, ArrayCount(CurrentCopy.File.Path.Absolute), false).FilePart;
		PathCopy(FilePart, CurrentCopy.File.Path.FilePart);

		if (Platform->PathIsTopLevel(CurrentCopy.File.Path.Absolute)) {
			PathCopy(CurrentCopy.File.Path.Absolute, CurrentCopy.File.Path.FilePart);
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
