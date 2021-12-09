/* date = November 20th 2021 3:59 pm */

#ifndef PLORE_VIM_H
#define PLORE_VIM_H

struct plore_vim_context;
typedef struct plore_vim_context plore_vim_context;

#define VIM_COMMANDS \
PLORE_X(None,            "none",             "None")            \
PLORE_X(Incomplete,      "incomplete",       "Incomplete")      \
PLORE_X(CompleteInsert,  "complete_insert",  "Complete Insert") \
PLORE_X(MoveLeft,        "move_left",        "Move Left")       \
PLORE_X(MoveRight,       "move_right",       "Move Right")      \
PLORE_X(MoveUp,          "move_up",          "Move Up")         \
PLORE_X(MoveDown,        "move_down",        "Move Down")       \
PLORE_X(Yank,            "yank",             "Yank")            \
PLORE_X(ClearYank,       "clear_yank",       "Clear Yank")      \
PLORE_X(Paste,           "paste",            "Paste")           \
PLORE_X(SelectUp,        "select_up",        "Select Up")       \
PLORE_X(SelectDown,      "select_down",      "Select Down")     \
PLORE_X(JumpTop,         "jump_top",         "Jump To Top")     \
PLORE_X(JumpBottom,      "jump_bottom",      "Jump To Bottom")  \
PLORE_X(ISearch,         "isearch",          "ISearch")         \
PLORE_X(ChangeDirectory, "change_directory", "Change Directory")

#define PLORE_X(Name, Ignored1, _Ignored2) VimCommandType_##Name,
typedef enum vim_command_type {
	VIM_COMMANDS
	#undef PLORE_X
	VimCommandType_Count,
	_VimCommandType_ForceU64 = 0xFFFFFFFF,
} vim_command_type;


#define PLORE_X(_Ignored1, _Ignored2, String) String,
char *VimCommandStrings[] = {
	VIM_COMMANDS
};
#undef PLORE_X

typedef enum vim_mode {
	VimMode_None,
	VimMode_Normal,
	VimMode_Insert,
	VimMode_Count,
	_VimMode_ForceU64 = 0xFFFFFFFF,
} vim_mode;

typedef struct vim_command {
	vim_command_type Type;
	vim_mode Mode;
	u64 Scalar;
	char *Shell;
} vim_command;


typedef struct vim_key {
	plore_key Input;
	plore_key Modifier;
} vim_key;

typedef struct plore_vim_context {
	vim_key CommandKeys[32];
	u64 CommandKeyCount;
	u64 MaxCommandCount; // @Hardcode
	vim_mode Mode;
	vim_command ActiveCommand;
	memory_arena CommandArena;
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
		.Shell = "C:\\Users\\Evan\\",
		.Keys = {
			{
				.Input = PloreKey_G,
			},
			{
				.Input = PloreKey_H,
				.Modifier = PloreKey_Shift,
			},
		}
	},
	{
		.Type = VimCommandType_ChangeDirectory,
		.Shell = "C:\\plore\\",
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
		}
	},
	{
		.Type = VimCommandType_SelectUp,
		.Keys = {
			{
				.Input = PloreKey_Space,
				.Modifier = PloreKey_Shift,
			},
		}
	},
	{
		.Type = VimCommandType_SelectDown,
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
		}
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
	#if 0
	{
		.Type = VimCommandType_CommandMode,
		.Keys = {
			{
				.Input = PloreKey_Semicolon,
			},
		}
	},
	#endif
};


typedef void vim_command_function(plore_state *State, plore_vim_context *VimContext, plore_file_context *FileContext, vim_command Command);
#define PLORE_VIM_COMMAND(CommandName) void Do##CommandName(plore_state *State, plore_vim_context *VimContext, plore_file_context *FileContext, vim_command Command)

#endif //PLORE_VIM_H
