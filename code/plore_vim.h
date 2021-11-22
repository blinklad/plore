/* date = November 20th 2021 3:59 pm */

#ifndef PLORE_VIM_H
#define PLORE_VIM_H

typedef enum vim_command_type {
	VimCommandType_None,
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
	interact_state State;
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
	plore_key CommandKeys[32];
	u64 CommandKeyCount;
	vim_command VimCommands[10];
	u64 VimCommandCount;
	u64 Cursor;
	u64 MaxCommandCount; // @Hardcode
} plore_vim_context;


internal vim_command
MakeCommand(plore_vim_context *Context) {
	vim_command Result = {0};
	
	local plore_key YankCommand[] =      { PloreKey_Y, PloreKey_Y, };
	local plore_key PasteCommand[] =     { PloreKey_P, PloreKey_P, };
	local plore_key ClearYankCommand[] = { PloreKey_U, PloreKey_Y, };
	local plore_key MoveLeftCommand[] =  { PloreKey_H };
	local plore_key MoveRightCommand[] = { PloreKey_L };
	local plore_key MoveUpCommand[] =    { PloreKey_K };
	local plore_key MoveDownCommand[] =  { PloreKey_J };
	local plore_key SelectCommand[] =    { PloreKey_Space };
	
	
	plore_key *C = Context->CommandKeys;
	if (MemoryCompare(C, SelectCommand, ArrayCount(SelectCommand)) == 0) {
		Result.Type = VimCommandType_Select;
	} else if (MemoryCompare(C, MoveLeftCommand, ArrayCount(MoveLeftCommand)) == 0)  {
		Result.Type = VimCommandType_Movement;
		Result.Movement.Direction = Left;
	} else if (MemoryCompare(C, MoveRightCommand, ArrayCount(MoveRightCommand)) == 0) {
		Result.Type = VimCommandType_Movement;
		Result.Movement.Direction = Right;
	} else if (MemoryCompare(C, MoveUpCommand, ArrayCount(MoveUpCommand)) == 0) {
		Result.Type = VimCommandType_Movement;
		Result.Movement.Direction = Up;
	} else if (MemoryCompare(C, MoveDownCommand, ArrayCount(MoveDownCommand)) == 0) {
		Result.Type = VimCommandType_Movement;
		Result.Movement.Direction = Down;
	} else if (MemoryCompare(C, YankCommand, ArrayCount(YankCommand)) == 0) {
		Result.Type = VimCommandType_Yank;
	} else if (MemoryCompare(C, PasteCommand, ArrayCount(PasteCommand)) == 0) {
		Result.Type = VimCommandType_Paste;
	} else if (MemoryCompare(C, ClearYankCommand, ArrayCount(ClearYankCommand)) == 0) {
		Result.Type = VimCommandType_ClearYank;
	}
	
	
	return(Result);
}

#endif //PLORE_VIM_H
