/* date = November 20th 2021 3:59 pm */

#ifndef PLORE_VIM_H
#define PLORE_VIM_H

#define VIM_COMMANDS \
PLORE_X(None,           "None")            \
PLORE_X(Incomplete,     "Incomplete")      \
PLORE_X(NormalMode,     "Normal Mode")     \
PLORE_X(ISearchMode,    "ISearch Mode")    \
PLORE_X(CommandMode,    "Command Mode")    \
PLORE_X(CompleteCommand,"Complete Mode")   \
PLORE_X(MoveLeft,       "Move Left")       \
PLORE_X(MoveRight,      "Move Right")      \
PLORE_X(MoveUp,         "Move Up")         \
PLORE_X(MoveDown,       "Move Down")       \
PLORE_X(Yank,           "Yank")            \
PLORE_X(ClearYank,      "Clear Yank")      \
PLORE_X(Paste,          "Paste")           \
PLORE_X(SelectUp,       "Select Up")       \
PLORE_X(SelectDown,     "Select Down")     \
PLORE_X(JumpTop,        "Jump To Top")     \
PLORE_X(JumpBottom,     "Jump To Bottom")  \
PLORE_X(CompleteSearch, "Complete Search") \
PLORE_X(ChangeDirectory,"Change Directory")

#define PLORE_X(Name, _Ignored) VimCommandType_##Name,
typedef enum vim_command_type {
	VIM_COMMANDS
	#undef PLORE_X
	VimCommandType_Count,
	_VimCommandType_ForceU64 = 0xFFFFFFFF,
} vim_command_type;


#define PLORE_X(_Ignored, String) String,
char *VimCommandStrings[] = {
	VIM_COMMANDS
};
#undef PLORE_X

typedef enum vim_mode {
	VimMode_Normal,
	VimMode_ISearch,
	VimMode_Command,
	VimMode_Count,
	_VimMode_ForceU64 = 0xFFFFFFFF,
} vim_mode;

typedef struct vim_command {
	u64 Scalar;
	vim_command_type Type;
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
	char Shell[256];
	u64 ShellCount;
} plore_vim_context;

typedef struct vim_binding {
	vim_key Keys[32];
	vim_command_type Type;
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
		.Type = VimCommandType_ISearchMode,
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
	{
		.Type = VimCommandType_CommandMode,
		.Keys = {
			{
				.Input = PloreKey_Semicolon,
			},
		}
	},
};


#endif //PLORE_VIM_H
