/* date = November 20th 2021 3:59 pm */

#ifndef PLORE_VIM_H
#define PLORE_VIM_H

#define VIM_COMMANDS \
PLORE_X(None,           "None")           \
PLORE_X(Incomplete,     "Incomplete")     \
PLORE_X(ISearchMode,    "ISearch Mode")   \
PLORE_X(NormalMode,     "Normal Mode")    \
PLORE_X(MoveLeft,       "Move Left")      \
PLORE_X(MoveRight,      "Move Right")     \
PLORE_X(MoveUp,         "Move Up")        \
PLORE_X(MoveDown,       "Move Down")      \
PLORE_X(Yank,           "Yank")           \
PLORE_X(ClearYank,      "Clear Yank")     \
PLORE_X(Paste,          "Paste")          \
PLORE_X(SelectUp,       "Select Up")      \
PLORE_X(SelectDown,     "Select Down")    \
PLORE_X(JumpTop,        "Jump To Top")    \
PLORE_X(JumpBottom,     "Jump To Bottom") \
PLORE_X(CompleteSearch, "Complete Search")

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
	VimMode_Count,
	_VimMode_ForceU64 = 0xFFFFFFFF,
} vim_mode;

typedef struct vim_command {
	u64 Scalar;
	vim_command_type Type;
} vim_command;


typedef struct vim_key {
	plore_key Input;
	plore_key Modifier;
	b64 DisableBufferedMove;
} vim_key;


typedef struct plore_vim_context {
	vim_key CommandKeys[32];
	u64 CommandKeyCount;
	u64 MaxCommandCount; // @Hardcode
	
	vim_command VimCommandHistory[10];
	u64 VimCommandHistoryCount;
	vim_mode Mode;
} plore_vim_context;

typedef struct vim_binding {
	vim_key Keys[32];
	vim_command_type Type;
	b64 DisableBufferedMove;
} vim_binding;

global vim_binding VimBindings[] = {
	{
		.Type = VimCommandType_Yank,
		.Keys = {
			[0] = {
				.Input = PloreKey_Y,
			},
			[1] = {
				.Input = PloreKey_Y,
			},
		}
	},
	{
		.Type = VimCommandType_ClearYank,
		.Keys = {
			[0] = {
				.Input = PloreKey_U,
			},
			[1] = {
				.Input = PloreKey_Y,
			},
		}
	},
	{
		.Type = VimCommandType_SelectUp,
		.Keys = {
			[0] = {
				.Input = PloreKey_Space,
				.Modifier = PloreKey_Shift,
			},
		}
	},
	{
		.Type = VimCommandType_SelectDown,
		.Keys = {
			[0] = {
				.Input = PloreKey_Space,
			},
		}
	},
	{
		.Type = VimCommandType_Paste,
		.Keys = {
			[0] = {
				.Input = PloreKey_P,
			},
			[1] = {
				.Input = PloreKey_P,
			},
		}
	},
	{
		.Type = VimCommandType_MoveLeft,
		.Keys = {
			[0] = {
				.Input = PloreKey_H,
			},
		}
	},
	{
		.Type = VimCommandType_MoveRight,
		.DisableBufferedMove = true,
		.Keys = {
			[0] = {
				.Input = PloreKey_L,
			},
		}
	},
	{
		.Type = VimCommandType_MoveUp,
		.Keys = {
			[0] = {
				.Input = PloreKey_K,
			},
		}
	},
	{
		.Type = VimCommandType_MoveDown,
		.Keys = {
			[0] = {
				.Input = PloreKey_J,
			},
		}
	},
	{
		.Type = VimCommandType_ISearchMode,
		.Keys = {
			[0] = {
				.Input = PloreKey_Slash,
			},
		}
	},
	{
		.Type = VimCommandType_JumpTop,
		.Keys = {
			[0] = {
				.Input = PloreKey_G,
			},
			[1] = {
				.Input = PloreKey_G,
			},
		}
	},
	{
		.Type = VimCommandType_JumpBottom,
		.Keys = {
			[0] = {
				.Input = PloreKey_G,
				.Modifier = PloreKey_Shift,
			},
		}
	},
};


#endif //PLORE_VIM_H
