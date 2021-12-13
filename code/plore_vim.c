typedef struct make_command_result {
	vim_command Command;
	b64 Candidates[ArrayCount(VimBindings)];
	b64 CandidateCount;
} make_command_result;


internal void
SetActiveCommand(plore_vim_context *Context, vim_command Command) {
	Context->ActiveCommand = Command;
}

internal void
ClearCommands(plore_vim_context *Context) {
	MemoryClear(Context->CommandKeys, sizeof(Context->CommandKeys));
	Context->CommandKeyCount = 0;
}

plore_inline vim_key *
PeekLatestKey(plore_vim_context *Context) {
	vim_key *Result = 0;
	if (Context->CommandKeyCount) {
		Result = Context->CommandKeys + Context->CommandKeyCount-1;
	}
	
	return(Result);
}

typedef struct string_to_command_result {
	vim_key CommandKeys[CommandBufferSize];
	u64 CommandKeyCount;
} string_to_command_result;

internal string_to_command_result
StringToCommand(char *S) {
	string_to_command_result Result = {0};
	
	while (*S) {
		char C = *S++;
		Result.CommandKeys[Result.CommandKeyCount++] = (vim_key) {
			.Input = GetKey(C),
			.Modifier = IsUpper(C) ? PloreKey_Shift : PloreKey_None, // @Cleanup
		};
	}
	return(Result);
}

internal make_command_result
MakeCommand(plore_vim_context *Context) {
	make_command_result Result = {
		.Command = {
			.Scalar = 1,
		},
	};
	if (!Context->CommandKeyCount) return(Result);
	
	vim_key *C = Context->CommandKeys;
	
	switch (Context->Mode) {
		case VimMode_Normal: {
			char Buffer[ArrayCount(Context->CommandKeys) + 1] = {0};
			u64 BufferCount = 0;
			while (IsNumeric(PloreKeyCharacters[C->Input])) {
				Buffer[BufferCount++] = PloreKeyCharacters[C->Input];
				C++;
				
			}
			if (BufferCount) {
				Result.Command.Scalar = StringToI32(Buffer);
				Result.Command.State = VimCommandState_Incomplete;
			} 
			
			if (BufferCount < Context->CommandKeyCount) {
				// NOTE(Evan): If there's at least one non-scalar key, the command can't be incomplete - there must be at least one candidate
				// command, which includes substrings, otherwise the command buffer should be cleared as there's no possible command that can be matched.
				
				for (u64 B = 0; B < ArrayCount(VimBindings); B++) {
					vim_binding *Binding = VimBindings + B;
					b64 PotentialCandidate = true;
					for (u64 K = 0; K < ArrayCount(Binding->Keys); K++) {
						if (!Binding->Keys[K].Input) break;
						
						for (u64 Command = 0; Command < (ArrayCount(Context->CommandKeys)-BufferCount); Command++) {
							if (Context->CommandKeyCount-BufferCount-Command == 0) break;
							if (Binding->Keys[K+Command].Input != C[K+Command].Input) {
								PotentialCandidate = false;
								break;
							}
							
							if (Binding->Keys[K+Command].Modifier != C[K+Command].Modifier) {
								PotentialCandidate = false;
								break;
							}
						}
						
						if (PotentialCandidate) {
							Result.Command.State = VimCommandState_Incomplete;
							Result.Candidates[B] = true;
							Result.CandidateCount += 1;
							break;
						}
					}
				}
			}
			
			if (Result.CandidateCount) {
				for (u64 Candidate = 0; Candidate < ArrayCount(Result.Candidates); Candidate++) {
					if (Result.Candidates[Candidate]) {
						u64 NonScalarEntries = Context->CommandKeyCount-BufferCount;
						u64 CommandLength;
						for (CommandLength = 0; CommandLength < ArrayCount(VimBindings[Candidate].Keys); CommandLength++) {
							if (!VimBindings[Candidate].Keys[CommandLength].Input) break;
							
						}
						if (NonScalarEntries < CommandLength) continue;
						
						if (MemoryCompare(VimBindings[Candidate].Keys, C, NonScalarEntries * sizeof(vim_key)) == 0) {
							Result.Command.Type = VimBindings[Candidate].Type;
							Result.Command.Mode = VimBindings[Candidate].Mode;
							Result.Command.Shell = VimBindings[Candidate].Shell;
							Result.Command.State = VimCommandState_Start;
							break;
						} 
					}
				}
			}
				
		} break;
		
		case VimMode_Insert: {
			Assert(Context->ActiveCommand.Type);
			
			vim_key *LatestKey = PeekLatestKey(Context);
			if (LatestKey) {
				if (Context->CommandKeyCount == ArrayCount(Context->CommandKeys)) {
					DrawText("Command key count reached, changing to normal mode.");
					Context->Mode = VimMode_Normal;
					Context->ActiveCommand = ClearStruct(vim_command);
				} else if (LatestKey->Input == PloreKey_Return) {
					Result.Command.Type = Context->ActiveCommand.Type;
					Result.Command.State = VimCommandState_Finish;
					Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key); // NOTE(Evan): Remove return from command buffer.
				} else {
					if (LatestKey->Input == PloreKey_Backspace) {
						if (Context->CommandKeyCount > 1) {
							Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key);
						}
						Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key);
					} 
					
					Result.Command.Type = Context->ActiveCommand.Type;
					Result.Command.State = VimCommandState_Incomplete;
				}
			}
		} break;
		
		case VimMode_Lister: {
			Assert(Context->ActiveCommand.Type);
			
			Result.Command.Type = Context->ActiveCommand.Type;
			vim_key *LatestKey = PeekLatestKey(Context);
			
			// @Cutnpaste from VimMode_Insert
			// TODO(Evan): Lister doesn't support insertions, but it may be possible to fold other movement with insert mode.
			if (LatestKey) {
				if (Context->CommandKeyCount == ArrayCount(Context->CommandKeys)) {
					DrawText("Command key count reached, changing to normal mode.");
					Context->Mode = VimMode_Normal;
					Context->ActiveCommand = ClearStruct(vim_command);
				} else if (LatestKey->Input == PloreKey_Return) {
					Result.Command.Type = Context->ActiveCommand.Type;
					Result.Command.State = VimCommandState_Finish;
					Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key); // NOTE(Evan): Remove return from command buffer.
				} else {
					// TODO(Evan): Lister cursor movement.
					Result.Command.Type = Context->ActiveCommand.Type;
					Result.Command.State = VimCommandState_Incomplete;
				}
			}
			
			
		} break;
		
		InvalidDefaultCase;
	}
	
	return(Result);
}

PLORE_VIM_COMMAND(None) {
	Assert(!"None command function reached!");
}

PLORE_VIM_COMMAND(MoveLeft) {
	u64 ScalarCount = 0;
	while (Command.Scalar--) {
		char Buffer[PLORE_MAX_PATH] = {0};
		Platform->GetCurrentDirectory(Buffer, PLORE_MAX_PATH);
		
		Print("Moving up a directory, from %s ", Buffer);
		Platform->PopPathNode(Buffer, PLORE_MAX_PATH, false);
		PrintLine("to %s", Buffer);
		
		Platform->SetCurrentDirectory(Buffer);
		ScalarCount++;
		
		if (Platform->IsPathTopLevel(Buffer, PLORE_MAX_PATH)) break;
	}
	Command.Scalar = ScalarCount;
}

PLORE_VIM_COMMAND(MoveRight) {
	u64 WasGivenScalar = Command.Scalar > 1;
	u64 ScalarCount = 0;
	while (Command.Scalar--) {
		plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(FileContext, &State->DirectoryState.Current.File.Path);
		plore_file *CursorEntry = State->DirectoryState.Current.Entries + CursorResult.Cursor->Cursor;
		
		if (CursorEntry->Type == PloreFileNode_Directory) {
			PrintLine("Moving down a directory, from %s to %s", 
					  State->DirectoryState.Current.File.Path.Absolute, 
					  CursorEntry->Path.Absolute);
			Platform->SetCurrentDirectory(CursorEntry->Path.Absolute);
			
			ScalarCount++;
			
			if (Platform->IsPathTopLevel(CursorEntry->Path.Absolute, PLORE_MAX_PATH)) return;
			else SynchronizeCurrentDirectory(FileContext, &State->DirectoryState);
		} else if (!WasGivenScalar) {
			if (CursorEntry->Extension) {
				u64 HandlerCount = GetHandlerCount(CursorEntry->Extension);
				
				if (HandlerCount > 1) {
					SetActiveCommand(VimContext, (vim_command) {
										 .Type = VimCommandType_OpenFile,
									 });
					VimContext->Mode = VimMode_Lister;
					int BreakHere = 5;
				} else {
					Platform->RunShell(PloreFileExtensionHandlers[CursorEntry->Extension]->Shell, CursorEntry->Path.Absolute);
				}
			} else {
				DrawText("Unknown extension - specify a handler", CursorEntry->Path.FilePart);
			}
		}
		
		if (!State->DirectoryState.Current.Count) return;
		
	}
	Command.Scalar = ScalarCount;
}

PLORE_VIM_COMMAND(MoveUp) {
	if (State->DirectoryState.Current.Count) {
		plore_file_listing_cursor_get_or_create_result CurrentResult = GetOrCreateCursor(FileContext, &State->DirectoryState.Current.File.Path);
		u64 Magnitude = Command.Scalar % State->DirectoryState.Current.Count;
		if (CurrentResult.Cursor->Cursor == 0) {
			CurrentResult.Cursor->Cursor = State->DirectoryState.Current.Count-Magnitude;
		} else {
			CurrentResult.Cursor->Cursor = (CurrentResult.Cursor->Cursor - Magnitude) % State->DirectoryState.Current.Count;
		}
		
	}
}

PLORE_VIM_COMMAND(MoveDown)  {
	if (State->DirectoryState.Current.Count) {
		plore_file_listing_cursor_get_or_create_result CurrentResult = GetOrCreateCursor(FileContext, &State->DirectoryState.Current.File.Path);
		u64 Magnitude = Command.Scalar % State->DirectoryState.Current.Count;
		
		CurrentResult.Cursor->Cursor = (CurrentResult.Cursor->Cursor + Magnitude) % State->DirectoryState.Current.Count;
	}
}

PLORE_VIM_COMMAND(Yank)  {
	if (FileContext->SelectedCount > 1) { // Yank selection if there is any.
		MemoryCopy(FileContext->Selected, FileContext->Yanked, sizeof(FileContext->Yanked));
		FileContext->YankedCount = FileContext->SelectedCount;
	} else { 
		plore_path *Selected = GetImpliedSelection(State);
		if (Selected) {
			MemoryCopy(Selected, FileContext->Yanked, sizeof(FileContext->Yanked));
			FileContext->YankedCount = 1;
		}
	}
	
}

PLORE_VIM_COMMAND(ClearYank)  {
	if (FileContext->YankedCount) {
		FileContext->YankedCount = 0;
		FileContext->SelectedCount = 0;
	}
	// TODO(Evan): Invalidate the yankees' directory's cursor.
}

PLORE_VIM_COMMAND(Paste)  {
	if (FileContext->YankedCount) { // @Cleanup
		u64 PastedCount = 0;
		plore_file_listing *Current = &State->DirectoryState.Current;
		u64 WeAreTopLevel = Platform->IsPathTopLevel(Current->File.Path.Absolute, PLORE_MAX_PATH);
		for (u64 M = 0; M < FileContext->YankedCount; M++) {
			plore_path *Movee = FileContext->Yanked + M;
			
			char NewPath[PLORE_MAX_PATH] = {0};
			u64 BytesWritten = CStringCopy(Current->File.Path.Absolute, NewPath, PLORE_MAX_PATH);
			Assert(BytesWritten < PLORE_MAX_PATH - 1);
			if (!WeAreTopLevel) {
				NewPath[BytesWritten++] = '\\';
			}
			CStringCopy(Movee->FilePart, NewPath + BytesWritten, PLORE_MAX_PATH);
			
			b64 DidMoveOk = Platform->MoveFile(Movee->Absolute, NewPath);
			if (DidMoveOk) {
				PastedCount++;
			} else {
				DrawText("Couldn't paste `%s`", Movee->FilePart);
			}
		}
		DrawText("Pasted %d items!", PastedCount);
		
		
		FileContext->YankedCount = 0;
		MemoryClear(FileContext->Yanked, sizeof(FileContext->Yanked));
		
		FileContext->SelectedCount = 0;
		MemoryClear(FileContext->Selected, sizeof(FileContext->Selected));
	}
}

internal void DoSelectHelper(plore_state *State, vim_command Command, i64 Direction) {
	plore_file_context *FileContext = State->FileContext;
	plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(FileContext, &State->DirectoryState.Current.File.Path);
	plore_file *Selectee = State->DirectoryState.Current.Entries + CursorResult.Cursor->Cursor;
	u64 ScalarCount = 0;
	u64 StartScalar = CursorResult.Cursor->Cursor;
	while (Command.Scalar--) {
		ToggleSelected(FileContext, &State->DirectoryState.Cursor.File.Path);
		if (Direction == +1) {
			CursorResult.Cursor->Cursor = (CursorResult.Cursor->Cursor + 1) % State->DirectoryState.Current.Count;
		} else {
			if (CursorResult.Cursor->Cursor == 0) {
				CursorResult.Cursor->Cursor = State->DirectoryState.Current.Count - 1;
			} else {
				CursorResult.Cursor->Cursor -= 1;
			}
		}
		Selectee = State->DirectoryState.Current.Entries + CursorResult.Cursor->Cursor;
		
		ScalarCount++;
		if (CursorResult.Cursor->Cursor == StartScalar) break;
	}
}

PLORE_VIM_COMMAND(SelectUp)  {
	DoSelectHelper(State, Command, -1);
}

PLORE_VIM_COMMAND(SelectDown)  {
	DoSelectHelper(State, Command, +1);
}
PLORE_VIM_COMMAND(JumpTop)  {
	plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(FileContext, &State->DirectoryState.Current.File.Path);
	CursorResult.Cursor->Cursor = 0;
}

PLORE_VIM_COMMAND(JumpBottom)  {
	plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(FileContext, &State->DirectoryState.Current.File.Path);
	if (State->DirectoryState.Current.Count) {
		CursorResult.Cursor->Cursor = State->DirectoryState.Current.Count-1;
	}
}

PLORE_VIM_COMMAND(ISearch)  {
	if (Command.Shell) {
		if (State->DirectoryState.Current.Count) {
			for (u64 F = 0; F < State->DirectoryState.Current.Count; F++) {
				plore_file *File = State->DirectoryState.Current.Entries + F;
				
				// TODO(Evan): Filter highlighting.
				if (Substring(File->Path.FilePart, State->DirectoryState.Filter).IsContained) {
					plore_file_listing_cursor *CurrentCursor = GetCursor(FileContext, State->DirectoryState.Current.File.Path.Absolute);
					CurrentCursor->Cursor = F;
					MemoryClear(State->DirectoryState.Filter, ArrayCount(State->DirectoryState.Filter));
					break;
				}
			}
		}
	} else {
		switch (Command.State) {
			case VimCommandState_Start: {
				ClearCommands(VimContext);
				VimContext->Mode = VimMode_Insert;
				SetActiveCommand(VimContext, Command);
			} break;
			
			case VimCommandState_Finish: {
			} break;
			
		}
		
	}
}

PLORE_VIM_COMMAND(ChangeDirectory) {
	if (Command.Shell) {
		Platform->SetCurrentDirectory(Command.Shell);
	} else {
		switch (Command.State) {
			case VimCommandState_Start: {
				ClearCommands(VimContext);
				VimContext->Mode = VimMode_Insert;
				SetActiveCommand(VimContext, Command);
			} break;
			
			case VimCommandState_Incomplete: {
			} break;
			
			InvalidDefaultCase;
		}
		
	}
}

PLORE_VIM_COMMAND(RenameFile) {
	plore_path *Selected = GetImpliedSelection(State);
	if (!Selected) return;

	switch (Command.State) {
		case VimCommandState_Start: {
			string_to_command_result Result = StringToCommand(Selected->FilePart);
			MemoryCopy(Result.CommandKeys, VimContext->CommandKeys, sizeof(VimContext->CommandKeys));
			VimContext->CommandKeyCount = Result.CommandKeyCount;
			
			VimContext->Mode = VimMode_Insert;
			Command.State = VimCommandState_Start;
			SetActiveCommand(VimContext, Command);
			
			DrawText("Start");
		} break;
		
		case VimCommandState_Incomplete: {
			DrawText("Incomplete");
		} break;
		
		case VimCommandState_Finish: {
			if (Command.Shell && *Command.Shell) {
				char Buffer[PLORE_MAX_PATH] = {0};
				CStringCopy(Selected->Absolute, Buffer, ArrayCount(Buffer));
				Platform->PopPathNode(Buffer, ArrayCount(Buffer), true);
				char *NewAbsolute = StringConcatenate(Buffer, ArrayCount(Buffer), Command.Shell);
				if (Platform->RenameFile(Selected->Absolute, NewAbsolute)) {
					FileContext->SelectedCount = 0;
				}
			}
		} break;
		
		InvalidDefaultCase;
	} 
	
}

PLORE_VIM_COMMAND(Select) {
	plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(FileContext, &State->DirectoryState.Current.File.Path);
	plore_file *Selectee = State->DirectoryState.Current.Entries + CursorResult.Cursor->Cursor;
	ToggleSelected(FileContext, &State->DirectoryState.Cursor.File.Path);
}

PLORE_VIM_COMMAND(OpenFile) {
	switch (Command.State) {
		case VimCommandState_Start: {
			DrawText("start");
		} break;
		
		case VimCommandState_Finish: {
			DrawText("finish");
			if (Command.Shell) {
				DrawText("shell ok");
			}
		} break;
	}
}
	
#define PLORE_X(Name, Ignored1, _Ignored2) Do##Name,
vim_command_function *VimCommands[] = {
	VIM_COMMANDS
};
#undef PLORE_X


