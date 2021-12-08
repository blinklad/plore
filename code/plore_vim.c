typedef struct make_command_result {
	vim_command Command;
	b64 Candidates[ArrayCount(VimBindings)];
	b64 CandidateCount;
} make_command_result;


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
							Result.Command.Shell = VimBindings[Candidate].Shell;
							break;
						} 
					}
				}
			}
				
		} break;
		
		// @Cleanup
		// This could be folded with Command, as its just text editing functionality.
		case VimMode_ISearch: {
			vim_key *LatestKey = PeekLatestKey(Context);
			if (LatestKey) {
				if (Context->CommandKeyCount == ArrayCount(Context->CommandKeys)) {
					DrawText("Command key count reached, changing to normal mode.");
					Result.Command.Type = VimCommandType_NormalMode;
				} else if (LatestKey->Input == PloreKey_Return) {
					Result.Command.Type = VimCommandType_CompleteSearch;
					Context->CommandKeys[--Context->CommandKeyCount] = (vim_key) {0}; // NOTE(Evan): Remove return from command buffer.
				} else {
					if (LatestKey->Input == PloreKey_Backspace) {
						if (Context->CommandKeyCount > 1) {
							Context->CommandKeys[--Context->CommandKeyCount] = (vim_key) {0};
						}
						Context->CommandKeys[--Context->CommandKeyCount] = (vim_key) {0};
					} 
					if (Context->CommandKeyCount) {
						Result.Command.Type = VimCommandType_Incomplete;
					}
				}
			}
			
		} break;
		
		// @Cleanup
		// Incomplete is very overloaded and inconsistent between otherwise identical cases for ISearch and Command.
		case VimMode_Command: {
			vim_key *LatestKey = PeekLatestKey(Context);
			// @Cutnpaste from above
			if (LatestKey) {
				if (Context->CommandKeyCount == ArrayCount(Context->CommandKeys)) {
					DrawText("Command key count reached, changing to normal mode.");
					Result.Command.Type = VimCommandType_NormalMode;
				} else if (LatestKey->Input == PloreKey_Return) {
					Result.Command.Type = VimCommandType_CompleteCommand;
					Context->CommandKeys[--Context->CommandKeyCount] = (vim_key) {0}; // NOTE(Evan): Remove return from command buffer.
				} else {
					if (LatestKey->Input == PloreKey_Backspace) {
						if (Context->CommandKeyCount > 1) {
							Context->CommandKeys[--Context->CommandKeyCount] = (vim_key) {0};
						}
						Context->CommandKeys[--Context->CommandKeyCount] = (vim_key) {0};
					} 
					
					Result.Command.Type = VimCommandType_Incomplete;
				}
			}
		} break;
		
		InvalidDefaultCase;
	}
	
	return(Result);
}
