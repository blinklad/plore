internal void
PushVimCommand(plore_vim_context *Context, vim_command Command) {
	Context->VimCommands[Context->VimCommandCount++ % ArrayCount(Context->VimCommands)] = Command;
}
	