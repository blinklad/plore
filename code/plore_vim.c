typedef struct make_command_result {
	vim_command Command;
	b64 Candidates[ArrayCount(VimBindings)];
	b64 CandidateCount;
} make_command_result;


internal void
SetActiveCommand(plore_vim_context *Context, vim_command Command) {
	Context->ActiveCommand = Command;
}

plore_inline vim_key *
PeekLatestKey(plore_vim_context *Context) {
	vim_key *Result = 0;
	if (Context->CommandKeyCount) {
		Result = Context->CommandKeys + Context->CommandKeyCount-1;
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
				Result.Command.Type = VimCommandType_Incomplete;
			} 
			
			if (BufferCount < Context->CommandKeyCount) {
				// NOTE(Evan): If there's at least one non-scalar key, the command can't be incomplete - there must be at least one candidate
				// command, which includes substrings, otherwise the command buffer should be cleared as there's no possible command that can be matched.
				Result.Command.Type = VimCommandType_None;
				
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
							Result.Command.Type = VimCommandType_Incomplete;
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
							break;
						} 
					}
				}
			}
				
		} break;
		
		case VimMode_Insert: {
			vim_key *LatestKey = PeekLatestKey(Context);
			// @Cutnpaste from above
			if (LatestKey) {
				if (Context->CommandKeyCount == ArrayCount(Context->CommandKeys)) {
					DrawText("Command key count reached, changing to normal mode.");
					Result.Command.Mode = VimMode_Normal;
				} else if (LatestKey->Input == PloreKey_Return) {
					Result.Command.Type = VimCommandType_CompleteInsert;
					Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key); // NOTE(Evan): Remove return from command buffer.
				} else {
					if (LatestKey->Input == PloreKey_Backspace) {
						if (Context->CommandKeyCount > 1) {
							Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key);
						}
						Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key);
					} 
					
					Result.Command.Type = VimCommandType_Incomplete;
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

PLORE_VIM_COMMAND(Incomplete) {
	// NOTE(Evan): Set the text filter that's used below and in the GUI.
	VimKeysToString(State->DirectoryState.Filter, 
					ArrayCount(State->DirectoryState.Filter), 
					VimContext);
	
}

PLORE_VIM_COMMAND(CompleteInsert) {
	VimKeysToString(State->DirectoryState.Filter, 
					ArrayCount(State->DirectoryState.Filter), 
					VimContext);
	if (Command.Type == VimCommandType_CompleteInsert) {
		VimContext->ActiveCommand.Shell = PloreKeysToString(&VimContext->CommandArena, 
															VimContext->CommandKeys, 
															VimContext->CommandKeyCount);
		VimContext->Mode = VimMode_Normal;
		VimContext->ActiveCommand = ClearStruct(vim_command);
	}
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
		plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(State->FileContext, &State->DirectoryState.Current.File.Path);
		plore_file *CursorEntry = State->DirectoryState.Current.Entries + CursorResult.Cursor->Cursor;
		
		if (CursorEntry->Type == PloreFileNode_Directory) {
			PrintLine("Moving down a directory, from %s to %s", 
					  State->DirectoryState.Current.File.Path.Absolute, 
					  CursorEntry->Path.Absolute);
			Platform->SetCurrentDirectory(CursorEntry->Path.Absolute);
			
			ScalarCount++;
			
			if (Platform->IsPathTopLevel(CursorEntry->Path.Absolute, PLORE_MAX_PATH)) return;
			else SynchronizeCurrentDirectory(State->FileContext, &State->DirectoryState);
		} else if (!WasGivenScalar) {
			if (CursorEntry->Extension != PloreFileExtension_Unknown) {
				DrawText("Opening %s", CursorEntry->Path.FilePart);
				Platform->RunShell(PloreFileExtensionHandlers[CursorEntry->Extension], CursorEntry->Path.Absolute);
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
		plore_file_listing_cursor_get_or_create_result CurrentResult = GetOrCreateCursor(State->FileContext, &State->DirectoryState.Current.File.Path);
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
		plore_file_listing_cursor_get_or_create_result CurrentResult = GetOrCreateCursor(State->FileContext, &State->DirectoryState.Current.File.Path);
		u64 Magnitude = Command.Scalar % State->DirectoryState.Current.Count;
		
		CurrentResult.Cursor->Cursor = (CurrentResult.Cursor->Cursor + Magnitude) % State->DirectoryState.Current.Count;
	}
}

PLORE_VIM_COMMAND(Yank)  {
	if (FileContext->SelectedCount) { // Yank selection if there is any.
		MemoryCopy(FileContext->Selected, FileContext->Yanked, sizeof(FileContext->Yanked));
		FileContext->YankedCount = FileContext->SelectedCount;
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
	plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(State->FileContext, &State->DirectoryState.Current.File.Path);
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
	plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(State->FileContext, &State->DirectoryState.Current.File.Path);
	CursorResult.Cursor->Cursor = 0;
}

PLORE_VIM_COMMAND(JumpBottom)  {
	plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(State->FileContext, &State->DirectoryState.Current.File.Path);
	if (State->DirectoryState.Current.Count) {
		CursorResult.Cursor->Cursor = State->DirectoryState.Current.Count-1;
	}
}

PLORE_VIM_COMMAND(ISearch)  {
	if (Command.Shell) {
		if (State->DirectoryState.Current.Count) {
			for (u64 F = 0; F < State->DirectoryState.Current.Count; F++) {
				plore_file *File = State->DirectoryState.Current.Entries + F;
				
				if (Substring(File->Path.FilePart, State->DirectoryState.Filter).IsContained) {
					plore_file_listing_cursor *CurrentCursor = GetCursor(State->FileContext, State->DirectoryState.Current.File.Path.Absolute);
					CurrentCursor->Cursor = F;
					MemoryClear(State->DirectoryState.Filter, ArrayCount(State->DirectoryState.Filter));
					break;
				}
			}
		}
	} else {
		VimContext->Mode = VimMode_Insert;
		SetActiveCommand(VimContext, Command);
	}
}

PLORE_VIM_COMMAND(ChangeDirectory) {
	if (Command.Shell) {
		Platform->SetCurrentDirectory(Command.Shell);
	} else {
		VimContext->Mode = VimMode_Insert;
		SetActiveCommand(VimContext, Command);
	}
}

#define PLORE_X(Name, Ignored1, _Ignored2) Do##Name,
vim_command_function *VimCommands[] = {
	VIM_COMMANDS
};
#undef PLORE_X


