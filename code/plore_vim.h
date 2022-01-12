/* date = November 20th 2021 3:59 pm */

#ifndef PLORE_VIM_H
#define PLORE_VIM_H

typedef struct plore_vim_context plore_vim_context;

//
//
// NOTE(Evan):
//
//      VimCommand               Command Name                    Command Description                                                                Insert Prompt (optional)
#define VIM_COMMANDS \
PLORE_X(None,                        "none",                          "None",                                                                            0)                                  \
PLORE_X(MoveLeft,                    "move_left",                     "Move cursor left, up directory tree",                                             0)                                  \
PLORE_X(MoveRight,                   "move_right",                    "Move cursor right, down directory tree or opening file under cursor",             0)                                  \
PLORE_X(MoveUp,                      "move_up",                       "Move up within current directory",                                                0)                                  \
PLORE_X(MoveDown,                    "move_down",                     "Move down within current directory",                                              0)                                  \
PLORE_X(PageDown,                    "page_down",                     "Move down one page",                                                              0)                                  \
PLORE_X(PageUp,                      "page_up",                       "Move up one page",                                                                0)                                  \
PLORE_X(JumpTop,                     "jump_top",                      "Jump to top of directory",                                                        0)                                  \
PLORE_X(JumpBottom,                  "jump_bottom",                   "Jump to bottom of directory",                                                     0)                                  \
PLORE_X(Yank,                        "yank",                          "Yank file under cursor",                                                          0)                                  \
PLORE_X(YankAll,                     "yank_all",                      "Yank all files in directory",                                                     0)                                  \
PLORE_X(ClearYank,                   "clear_yank",                    "Clear yank in all directories",                                                   0)                                  \
PLORE_X(Paste,                       "paste",                         "Paste yanked file/s in current directory",                                        0)                                  \
PLORE_X(Select,                      "select",                        "Select file under cursor",                                                        0)                                  \
PLORE_X(SelectUp,                    "select_up",                     "Select and move cursor upwards in current directory",                             0)                                  \
PLORE_X(SelectDown,                  "select_down",                   "Select and move cursor downwards in current directory",                           0)                                  \
PLORE_X(SelectAll,                   "select_all",                    "Toggle selection for all files in current directory",                             0)                                  \
PLORE_X(ClearSelect,                 "clear_select",                  "Clear select in all directories",                                                 0)                                  \
PLORE_X(ISearch,                     "isearch",                       "Search in current directory, highlighting matching files",                        "ISearch:")                         \
PLORE_X(TabTextFilterHide,           "tab_text_filter_hide",          "Set text filter, hiding all matching files in current tab",                       "Hide files in tab matching text:") \
PLORE_X(TabTextFilterShow,           "tab_text_filter_show",          "Set text filter, showing all matching files in current tab",                      "Show files in tab matching text:") \
PLORE_X(TabTextFilterClear,          "tab_text_filter_clear",         "Clear all text filters for current tab. ",                                        0)                                  \
PLORE_X(DirectoryTextFilterHide,     "directory_text_filter_hide",    "Set text filter, hiding all matching files in current directory",                 "Hide files in tab matching text:") \
PLORE_X(DirectoryTextFilterShow,     "directory_text_filter_show",    "Set text filter, showing all matching files in current directory",                "Show files in tab matching text:") \
PLORE_X(DirectoryTextFilterClear,    "directory_text_filter_clear",   "Clear all directory text filters. ",                                              0)                                  \
PLORE_X(ChangeDirectory,             "change_directory",              "Change directory/drive",                                                          "Change directory to?")             \
PLORE_X(RenameFile,                  "rename_file",                   "Rename file under cursor",                                                        "Rename file to?")                  \
PLORE_X(OpenFile,                    "open_file",                     "Displays all file extension handlers for file under cursor",                      0)                                  \
PLORE_X(OpenFileWith,                "open_file_with",                "Open file under cursor using specified shell command",                            "Open file using program: ")        \
PLORE_X(NewTab,                      "new_file",                      "Create new or switch to already existing tab with provided number",               0)                                  \
PLORE_X(CloseTab,                    "close_tab",                     "Close currently open tab",                                                        0)                                  \
PLORE_X(OpenShell,                   "open_shell",                    "Open shell in current directory",                                                 0)                                  \
PLORE_X(CreateFile,                  "create_file",                   "Create file, in current directory",                                               "Create file with name?")           \
PLORE_X(CreateDirectory,             "create_directory",              "Create directory, in current directory",                                          "Create directory with name?")      \
PLORE_X(ShowHiddenFiles,             "toggle_show_hidden_files",      "Toggle visibility of hidden files",                                               0)                                  \
PLORE_X(ToggleSortName,              "toggle_sort_by_name",           "Toggle sorting by name        (default descending)",                              0)                                  \
PLORE_X(ToggleSortSize,              "toggle_sort_by_size",           "Toggle sort by size           (default descending)",                              0)                                  \
PLORE_X(ToggleSortModified,          "toggle_sort_by_modified_date",  "Toggle sort by modified date  (default descending)",                              0)                                  \
PLORE_X(ToggleSortExtension,         "toggle_sort_by_extension",      "Toggle sort by extension      (default descending)",                              0)                                  \
PLORE_X(DeleteFile,                  "delete_file",                   "Delete file/s, with confirmation",                                                "Delete file/s? ('yes' to confirm)")\
PLORE_X(ShowCommandList,             "show_command_list",             "Show list of all commands",                                                       0)                                  \
PLORE_X(ExitPlore,                   "exit_plore",                    "Exit Plore",                                                                      0)                                  \
PLORE_X(FontScaleIncrease,           "font_scale_increase",           "Increase current font scale",                                                     0)                                  \
PLORE_X(FontScaleDecrease,           "font_scale_decrease",           "Decrease current font scale",                                                     0)                                  \
PLORE_X(ToggleExtendedFileMetadata,  "toggle_extended_file_metadata", "Toggle extended file metadata (directory size/count, file permissions, etc)",     0)                                  \
PLORE_X(ToggleFileMetadata,          "toggle_file_metadata",          "Toggle display of file metadata (size, date, etc)",                               0)                                  \

#define PLORE_X(Name, Ignored1, _Ignored2, _Ignored3) VimCommandType_##Name,
typedef enum vim_command_type {
	VIM_COMMANDS
	#undef PLORE_X
	VimCommandType_Count,
	_VimCommandType_ForceU64 = 0xFFFFFFFF,
} vim_command_type;


#define PLORE_X(_Ignored1, String, _Ignored2, _Ignored3) String,
char *VimCommandNames[] = {
	VIM_COMMANDS
};
#undef PLORE_X

#define PLORE_X(_Ignored1, _Ignored2, String, _Ignored3) String,
char *VimCommandDescriptions[] = {
	VIM_COMMANDS
};
#undef PLORE_X

#define PLORE_X(_Ignored1, _Ignored2, _Ignored3, String) String,
char *VimCommandInsertPrompts[] = {
	VIM_COMMANDS
};
#undef PLORE_X

typedef enum vim_pattern {
	VimPattern_None,
	VimPattern_Digit,
	VimPattern_Count,
	_VimPattern_ForceU64 = 0xFFFFFFFF,
} vim_pattern;

typedef enum vim_mode {
	VimMode_None,
	VimMode_Normal,
	VimMode_Insert,
	VimMode_Lister,
	VimMode_Count,
	_VimMode_ForceU64 = 0xFFFFFFFF,
} vim_mode;

typedef enum vim_command_state {
	VimCommandState_None,
	VimCommandState_Start,
	VimCommandState_Incomplete,
	VimCommandState_Cancel,
	VimCommandState_Finish,
	VimCommandState_Count,
	_VimCommandState_ForceU64 = 0xFFFFFFFF,
} vim_command_state;

typedef struct vim_command {
	vim_command_type Type;
	vim_command_state State;
	vim_mode Mode;
	u64 Scalar;
	
	// NOTE(Evan): Generated from non-scalar input, such as <ctrl> + 5.
	char Pattern[16];
	u64 PatternCount;
	
	char *Shell;
} vim_command;

typedef struct vim_key {
	plore_key Input;
	plore_key Modifier;
	vim_pattern Pattern;
} vim_key;

typedef enum vim_lister_mode {
	VimListerMode_Normal,
	VimListerMode_ISearch,
	_VimListerMode_ForceU64 = 0xfffffffful,
} vim_lister_mode;

enum { VimListing_ListSize = 128, VimListing_Size = 256 };
typedef struct vim_lister_state {
	char Titles[VimListing_ListSize][VimListing_Size];
	char Secondaries[VimListing_ListSize][VimListing_Size];
	u64 Count;
	u64 Cursor;
	vim_lister_mode Mode;
	b64 HideFiles;
} vim_lister_state;

#define CommandBufferSize 32
typedef struct plore_vim_context {
	vim_key CommandKeys[CommandBufferSize];
	u64 CommandKeyCount;
	
	vim_lister_state ListerState;
	vim_mode Mode;
	vim_command ActiveCommand;
} plore_vim_context;

typedef struct vim_binding {
	vim_key Keys[32];
	vim_command_type Type;
	vim_mode Mode;
	char *Shell;
} vim_binding;

global vim_binding VimBindings[] = {
	{
		.Type = VimCommandType_Yank,
		.Keys = {
			{
				.Input = PloreKey_Y,
			},
			{
				.Input = PloreKey_Y,
			},
		}
	},
	{
		.Type = VimCommandType_YankAll,
		.Keys = {
			{
				.Input = PloreKey_Y,
			},
			{
				.Input = PloreKey_G,
				.Modifier = PloreKey_Shift,
			},
		},
	},
	{
		.Type = VimCommandType_ClearYank,
		.Keys = {
			{
				.Input = PloreKey_U,
			},
			{
				.Input = PloreKey_Y,
			},
		}
	},
	{
		.Type = VimCommandType_ChangeDirectory,
		.Shell = PLORE_HOME,
		.Keys = {
			{
				.Input = PloreKey_G,
			},
			{
				.Input = PloreKey_H,
				.Modifier = PloreKey_Shift,
			},
		},
	},
	{
		.Type = VimCommandType_ChangeDirectory,
		.Shell = "C:/plore/", // @Hardcode
		.Keys = {
			{
				.Input = PloreKey_G,
			},
			{
				.Input = PloreKey_P,
			},
		}
	},
	{
		.Type = VimCommandType_ChangeDirectory,
		.Keys = {
			{
				.Input = PloreKey_C,
			},
			{
				.Input = PloreKey_D,
			},
		},
	},
	{
		.Type = VimCommandType_SelectDown,
		.Keys = {
			{
				.Input = PloreKey_Space,
				.Modifier = PloreKey_Shift,
			},
		}
	},
	{
		.Type = VimCommandType_SelectAll,
		.Keys = {
			{
				.Input = PloreKey_G,
			},
			{
				.Input = PloreKey_Space,
			},
		}
	},
	{
		.Type = VimCommandType_ClearSelect,
		.Keys = {
			{
				.Input = PloreKey_U,
			},
			{
				.Input = PloreKey_Space,
			},
		}
	},
	{
		.Type = VimCommandType_Select,
		.Keys = {
			{
				.Input = PloreKey_Space,
			},
		}
	},
	{
		.Type = VimCommandType_Paste,
		.Keys = {
			{
				.Input = PloreKey_P,
			},
			{
				.Input = PloreKey_P,
			},
		}
	},
	{
		.Type = VimCommandType_MoveLeft,
		.Keys = {
			{
				.Input = PloreKey_H,
			},
		}
	},
	{
		.Type = VimCommandType_MoveRight,
		.Keys = {
			{
				.Input = PloreKey_L,
			},
		}
	},
	{
		.Type = VimCommandType_MoveUp,
		.Keys = {
			{
				.Input = PloreKey_K,
			},
		}
	},
	{
		.Type = VimCommandType_MoveDown,
		.Keys = {
			{
				.Input = PloreKey_J,
			},
		}
	},
	{
		.Type = VimCommandType_PageUp,
		.Keys = {
			{
				.Input = PloreKey_U,
				.Modifier = PloreKey_Ctrl
			},
		}
	},
	{
		.Type = VimCommandType_PageDown,
		.Keys = {
			{
				.Input = PloreKey_D,
				.Modifier = PloreKey_Ctrl
			},
		}
	},
	{
		.Type = VimCommandType_ISearch,
		.Keys = {
			{
				.Input = PloreKey_Slash,
			},
		},
		
	},
	{
		.Type = VimCommandType_TabTextFilterHide,
		.Keys = {
			{
				.Input = PloreKey_F,
				.Modifier = PloreKey_Shift,
			},
			{
				.Input = PloreKey_T,
			},
			{
				.Input = PloreKey_H,
			},
		},
	},
	{
		.Type = VimCommandType_TabTextFilterShow,
		.Keys = {
			{
				.Input = PloreKey_F,
				.Modifier = PloreKey_Shift,
			},
			{
				.Input = PloreKey_T,
			},
			{
				.Input = PloreKey_S,
			},
		},
	},
	{
		.Type = VimCommandType_TabTextFilterClear,
		.Keys = {
			{
				.Input = PloreKey_U,
			},
			{
				.Input = PloreKey_T,
			},
			{
				.Input = PloreKey_F,
			},
		},
	},
	{
		.Type = VimCommandType_DirectoryTextFilterHide,
		.Keys = {
			{
				.Input = PloreKey_F,
				.Modifier = PloreKey_Shift,
			},
			{
				.Input = PloreKey_D,
			},
			{
				.Input = PloreKey_H,
			},
		},
	},
	{
		.Type = VimCommandType_DirectoryTextFilterShow,
		.Keys = {
			{
				.Input = PloreKey_F,
				.Modifier = PloreKey_Shift,
			},
			{
				.Input = PloreKey_D,
			},
			{
				.Input = PloreKey_S,
			},
		},
	},
	{
		.Type = VimCommandType_DirectoryTextFilterClear,
		.Keys = {
			{
				.Input = PloreKey_U,
			},
			{
				.Input = PloreKey_D,
			},
			{
				.Input = PloreKey_F,
			},
		},
	},
	{
		.Type = VimCommandType_JumpTop,
		.Keys = {
			{
				.Input = PloreKey_G,
			},
			{
				.Input = PloreKey_G,
			},
		}
	},
	{
		.Type = VimCommandType_JumpBottom,
		.Keys = {
			{
				.Input = PloreKey_G,
				.Modifier = PloreKey_Shift,
			},
		}
	},
	{
		.Type = VimCommandType_RenameFile,
		.Keys = {
			{
				.Input = PloreKey_R,
			},
			{
				.Input = PloreKey_R,
			},
		},
	},
	{
		.Type = VimCommandType_OpenFile,
		.Keys = {
			{
				.Input = PloreKey_O,
				.Modifier = PloreKey_Shift,
			},
		}
	},
	{
		.Type = VimCommandType_OpenFileWith,
		.Keys = {
			{
				.Input = PloreKey_O,
			},
			{
				.Input = PloreKey_O,
			},
		}
	},
	{
		.Type = VimCommandType_NewTab,
		.Keys = {
			{
				.Modifier = PloreKey_Ctrl,
				.Pattern = VimPattern_Digit,
			},
		}
	},
	{
		.Type = VimCommandType_CloseTab,
		.Keys = {
			{
				.Input = PloreKey_Q,
			},
			{
				.Input = PloreKey_T,
			},
		}
	},
	{
		.Type = VimCommandType_OpenShell,
		.Keys = {
			{
				.Input = PloreKey_S,
			},
			{
				.Input = PloreKey_S,
			},
		},
		.Shell = PLORE_TERMINAL,
	},
	{
		.Type = VimCommandType_OpenShell,
		.Keys = {
			{
				.Input = PloreKey_E,
				.Modifier = PloreKey_Shift,
			},
		},
		.Shell = PLORE_EDITOR,
	},
	{
		.Type = VimCommandType_CreateFile,
		.Keys = {
			{
				.Input = PloreKey_C,
			},
			{
				.Input = PloreKey_F,
			},
		},
	},
	{
		.Type = VimCommandType_CreateDirectory,
		.Keys = {
			{
				.Input = PloreKey_M,
			},
			{
				.Input = PloreKey_K,
			},
		},
	},
	{
		.Type = VimCommandType_ToggleSortName,
		.Keys = {
			{
				.Input = PloreKey_S,
				.Modifier = PloreKey_Shift,
			},
			{
				.Input = PloreKey_N,
			},
		},
	},
	{
		.Type = VimCommandType_ToggleSortSize,
		.Keys = {
			{
				.Input = PloreKey_S,
				.Modifier = PloreKey_Shift,
			},
			{
				.Input = PloreKey_S,
			},
		},
	},
	{
		.Type = VimCommandType_ToggleSortModified,
		.Keys = {
			{
				.Input = PloreKey_S,
				.Modifier = PloreKey_Shift,
			},
			{
				.Input = PloreKey_M,
			},
		},
	},
	{
		.Type = VimCommandType_ToggleSortExtension,
		.Keys = {
			{
				.Input = PloreKey_S,
				.Modifier = PloreKey_Shift,
			},
			{
				.Input = PloreKey_E,
			},
		},
	},
	{
		.Type = VimCommandType_ShowHiddenFiles,
		.Keys = {
			{
				.Input = PloreKey_H,
				.Modifier = PloreKey_Shift,
			},
		},
	},
	{
		.Type = VimCommandType_DeleteFile,
		.Keys = {
			
			{
				.Input = PloreKey_D,
			},
			{
				.Input = PloreKey_D,
				.Modifier = PloreKey_Shift,
			},
		},
	},
	{
		.Type = VimCommandType_MoveRight,
		.Keys = {
			
			{
				.Input = PloreKey_Return,
			},
		},
	},
	{
		.Type = VimCommandType_ShowCommandList,
		.Keys = {
			
			{
				.Input = PloreKey_QuestionMark,
			},
		},
	},
	{
		.Type = VimCommandType_ExitPlore,
		.Keys = {
			{
				.Input = PloreKey_Q,
			},
			{
				.Input = PloreKey_Q,
				.Modifier = PloreKey_Shift,
			},
		}
	},
	{
		.Type = VimCommandType_FontScaleIncrease,
		.Keys = {
			{
				.Input = PloreKey_P,
				.Modifier = PloreKey_Shift,
			},
		},
	},
	{
		.Type = VimCommandType_FontScaleDecrease,
		.Keys = {
			{
				.Input = PloreKey_N,
				.Modifier = PloreKey_Shift,
			},
		},
	},
	{
		.Type = VimCommandType_ToggleFileMetadata,
		.Keys = {
			{
				.Input = PloreKey_M,
			},
			{
				.Input = PloreKey_T,
			},
		},
	},
	{
		.Type = VimCommandType_ToggleExtendedFileMetadata,
		.Keys = {
			{
				.Input = PloreKey_M,
				.Modifier = PloreKey_Shift,
			},
			{
				.Input = PloreKey_T,
			},
		},
	}};


// NOTE(Evan): This could be generated from a metaprogram.
internal b64
AcceptsPattern(vim_binding *Binding) {
	b64 Result = false;
	for (u64 K = 0; K < ArrayCount(Binding->Keys); K++) {
		if (Binding->Keys[K].Pattern) {
			Result = true;
			break;
		}
	}
	
	return(Result);
}


#endif //PLORE_VIM_H
