/* date = November 20th 2021 3:59 pm */

#ifndef PLORE_VIM_H
#define PLORE_VIM_H

struct plore_vim_context;
typedef struct plore_vim_context plore_vim_context;

//
//
// NOTE(Evan):
//
//      VimCommand       Command Name        Command Display Name   Insert Prompt (optional)
#define VIM_COMMANDS \
PLORE_X(None,                "none",                         "None",                         0)                                 \
PLORE_X(MoveLeft,            "move_left",                    "Move Left",                    0)                                 \
PLORE_X(MoveRight,           "move_right",                   "Move Right",                   0)                                 \
PLORE_X(MoveUp,              "move_up",                      "Move Up",                      0)                                 \
PLORE_X(MoveDown,            "move_down",                    "Move Down",                    0)                                 \
PLORE_X(JumpTop,             "jump_top",                     "Jump To Top",                  0)                                 \
PLORE_X(JumpBottom,          "jump_bottom",                  "Jump To Bottom",               0)                                 \
PLORE_X(Yank,                "yank",                         "Yank",                         0)                                 \
PLORE_X(YankAll,             "yank_all",                     "Yank All",                     0)                                 \
PLORE_X(ClearYank,           "clear_yank",                   "Clear Yank",                   0)                                 \
PLORE_X(Paste,               "paste",                        "Paste",                        0)                                 \
PLORE_X(Select,              "select",                       "Select",                       0)                                 \
PLORE_X(SelectUp,            "select_up",                    "Select Up",                    0)                                 \
PLORE_X(SelectDown,          "select_down",                  "Select Down",                  0)                                 \
PLORE_X(SelectAll,           "select_all",                   "Select All",                   0)                                 \
PLORE_X(ISearch,             "isearch",                      "ISearch",                      "ISearch:")                        \
PLORE_X(ChangeDirectory,     "change_directory",             "Change Directory",             "Change directory to?")            \
PLORE_X(RenameFile,          "rename_file",                  "Rename File",                  "Rename file to?")                 \
PLORE_X(OpenFile,            "open_file",                    "Open File",                    0)                                 \
PLORE_X(NewTab,              "new_file",                     "New Tab",                      0)                                 \
PLORE_X(CloseTab,            "close_file",                   "Close Tab",                    0)                                 \
PLORE_X(OpenShell,           "open_shell",                   "Open Shell",                   0)                                 \
PLORE_X(CreateFile,          "create_file",                  "Create File",                  "Create file with name?")          \
PLORE_X(CreateDirectory,     "create_directory",             "Create Directory",             "Create directory with name?")     \
PLORE_X(ShowHiddenFiles,     "show_hidden_files",            "Show Hidden Files",            0)                                 \
PLORE_X(ToggleSortName,      "toggle_sort_by_name",          "Toggle Sort By Name",          0)                                 \
PLORE_X(ToggleSortSize,      "toggle_sort_by_size",          "Toggle Sort By Size",          0)                                 \
PLORE_X(ToggleSortModified,  "toggle_sort_by_modified_date", "Toggle Sort By Modified Date", 0)                                 \
PLORE_X(DeleteFile,          "delete_file",                  "Delete File",                  "Delete file? ('yes' to confirm)") \
PLORE_X(VerticalSplit,       "vertical_split",               "Vertical Split",               0)                                 \
PLORE_X(HorizontalSplit,     "horizontal_split",             "Horizontal Split",             0)

#define PLORE_X(Name, Ignored1, _Ignored2, _Ignored3) VimCommandType_##Name,
typedef enum vim_command_type {
	VIM_COMMANDS
	#undef PLORE_X
	VimCommandType_Count,
	_VimCommandType_ForceU64 = 0xFFFFFFFF,
} vim_command_type;


#define PLORE_X(_Ignored1, _Ignored2, String, _Ignored3) String,
char *VimCommandStrings[] = {
	VIM_COMMANDS
};
#undef PLORE_X

#define PLORE_X(_Ignored1, _Ignored2, _Ignored3, String) String,
char *VimInsertPrompts[] = {
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

#define CommandBufferSize 32
typedef struct plore_vim_context {
	vim_key CommandKeys[CommandBufferSize];
	u64 CommandKeyCount;
	
	u64 ListerCursor;
	u64 ListerCount;
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
				.Modifier = PloreKey_Ctrl,
			},
		}
	},
	{
		.Type = VimCommandType_SelectAll,
		.Keys = {
			{
				.Input = PloreKey_Space,
				.Modifier = PloreKey_Shift,
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
		.Type = VimCommandType_ISearch,
		.Keys = {
			{
				.Input = PloreKey_Slash,
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
			},
			{
				.Input = PloreKey_E,
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
		.Type = VimCommandType_VerticalSplit,
		.Keys = {
			{
				.Input = PloreKey_S,
			},
			{
				.Input = PloreKey_V,
			},
		},
	},
	{
		.Type = VimCommandType_HorizontalSplit,
		.Keys = {
			{
				.Input = PloreKey_S,
			},
			{
				.Input = PloreKey_H,
			},
		},
	},
};


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

typedef struct plore_tab plore_tab;

#define PLORE_VIM_COMMAND_FUNCTION(Name) \
void Name(plore_state *State, plore_tab *Tab, plore_vim_context *VimContext, plore_file_context *FileContext, vim_command Command)

typedef PLORE_VIM_COMMAND_FUNCTION(vim_command_function);
#define PLORE_VIM_COMMAND(CommandName) PLORE_VIM_COMMAND_FUNCTION(Do##CommandName)

#endif //PLORE_VIM_H
