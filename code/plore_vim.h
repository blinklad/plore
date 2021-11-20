/* date = November 20th 2021 3:59 pm */

#ifndef PLORE_VIM_H
#define PLORE_VIM_H

typedef enum vim_command_type {
	VimCommandType_Movement,
	VimCommandType_Yank,
	VimCommandType_ClearYank,
	VimCommandType_Paste,
	VimCommandType_Select,
	VimCommandType_ISearch,
	
	_VimCommandType_ForceU64 = 0xFFFFFFFF,
} vim_command_type;

typedef struct vim_command_movement {
	enum { Left, Right, Up, Down} Direction;
} vim_command_movement;

typedef struct vim_command_yank {
	u64 YankeeCount;
} vim_command_yank;

typedef struct vim_command_paste {
	u64 PasteeCount;
} vim_command_paste;

typedef struct vim_command_clear_yank {
	u64 YankeeCount;
} vim_command_clear_yank;

typedef struct vim_command_select {
	u64 SelectCount;
} vim_command_select;

typedef struct vim_command {
	vim_command_type Type;
	union {
		vim_command_movement   Movement;
		vim_command_yank       Yank;
		vim_command_paste      Paste;
		vim_command_clear_yank ClearYank;
		vim_command_select     Select;
	};
} vim_command;
typedef struct plore_vim_context {
	vim_command VimCommands[10];
	u64 VimCommandCount;
	u64 Cursor;
} plore_vim_context;


#endif //PLORE_VIM_H
