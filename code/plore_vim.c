typedef struct make_command_result {
	vim_command Command;
	b64 Candidates[ArrayCount(VimBindings)];
	u64 CandidateCount;
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

internal void
InsertBegin(plore_vim_context *VimContext, vim_command Command) {
	VimContext->Mode = VimMode_Insert;
	SetActiveCommand(VimContext, Command);
}

internal b64
Confirmation(char *S) {
	b64 Result = false;
	Result = CStringsAreEqualIgnoreCase(S, "y")   ||
		     CStringsAreEqualIgnoreCase(S, "yy")  ||
		     CStringsAreEqualIgnoreCase(S, "yes");
	
	return(Result);
}
			 
internal void
ResetVimState(plore_vim_context *Context) {
	ClearCommands(Context);
	ClearArena(&Context->CommandArena);
	Context->Mode = VimMode_Normal;
	Context->ActiveCommand = ClearStruct(vim_command);
	Context->ListerCursor = 0;
	Context->ListerCount = 0;
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
			
			// NOTE(Evan): Move the command buffer to the end of the contiguous scalar stream.
			char Buffer[ArrayCount(Context->CommandKeys) + 1] = {0};
			u64 BufferCount = 0;
			b64 HasScalar = false;
			b64 ScalarHasModifier = false;
			
			while (IsNumeric(PloreKeyCharacters[C->Input])) {
				if (C->Modifier) {
					ScalarHasModifier = true;
					break;
				}
				
				HasScalar = true;
				Buffer[BufferCount++] = PloreKeyCharacters[C->Input];
				C++;
			}
			
			if (C->Input == PloreKey_Return) return(Result);
			
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
						if (!Binding->Keys[K].Input && !Binding->Keys[K].Pattern) break;
						
						for (u64 Command = 0; Command < (ArrayCount(Context->CommandKeys)-BufferCount); Command++) {
							if (Context->CommandKeyCount-BufferCount-Command == 0) break;
							if (Binding->Keys[K+Command].Input != C[K+Command].Input) {
								PotentialCandidate = false;
								
								if (Binding->Keys[K+Command].Pattern) {
									if (Binding->Keys[K+Command].Pattern == C[K+Command].Pattern) {
										PotentialCandidate = true;
									}
								} else {
									break;
								}
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
						
						b64 StraightMatch = MemoryCompare(VimBindings[Candidate].Keys, C, NonScalarEntries * sizeof(vim_key) == 0);
						
						b64 PatternMatch = true;
						
						if (AcceptsPattern(VimBindings + Candidate)) {
							for (u64 K = 0; K < ArrayCount(VimBindings[Candidate].Keys); K++) {
								vim_key *Key = VimBindings[Candidate].Keys + K;
								
								if (Key->Pattern) {
									if (Key->Modifier != C[K].Modifier || Key->Pattern != C[K].Pattern) {
										PatternMatch = false;
										break;
									}
									
									Assert(Result.Command.PatternCount < ArrayCount(Result.Command.Pattern) - 1);
									Result.Command.Pattern[Result.Command.PatternCount++] = PloreKeyCharacters[C[K].Input];
								} else {
									if (MemoryCompare(C+K, Key, sizeof(vim_key)) != 0) {
										PatternMatch = false;
										break;
									}
								}
							}
						}
						
						if (StraightMatch || PatternMatch) {
							Result.Command.Type = VimBindings[Candidate].Type;
							Result.Command.Mode = VimBindings[Candidate].Mode;
							Result.Command.Shell = VimBindings[Candidate].Shell;
							Result.Command.State = VimCommandState_Start;
							break;
						} 
					}
				}
			} else {
				// NOTE(Evan): If there are no candidates trailing a scalar value, the command is not incomplete, it's just invalid.
				if (BufferCount < Context->CommandKeyCount && HasScalar) {
					Result.Command.State = VimCommandState_None;
				}
			}
			
		} break;
		
		case VimMode_Lister:
		case VimMode_Insert: {
			vim_key *LatestKey = PeekLatestKey(Context);
			Result.Command.Type = Context->ActiveCommand.Type;
			if (LatestKey) {
				if (Context->CommandKeyCount == ArrayCount(Context->CommandKeys)) {
					DrawText("Command key count reached, changing to normal mode.");
					ResetVimState(Context);
				} else if (LatestKey->Input == PloreKey_Return) {
					Result.Command.Type = Context->ActiveCommand.Type;
					Result.Command.State = VimCommandState_Finish;
					Context->ActiveCommand = Result.Command; // @Cleanup
					
					Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key); 
				} else {
					// NOTE(Evan): Mode-specific keymapping is here, as Return, Q and a full buffer terminate always.
					switch (Context->Mode) {
						case VimMode_Lister: {
							switch (LatestKey->Input) {
								case PloreKey_Q: {
									ResetVimState(Context);
								} break;
								case PloreKey_J:
								case PloreKey_K: {
									i64 Direction = LatestKey->Input == PloreKey_J ? +1 : -1;
									if (Direction < 0 && Context->ListerCursor == 0) {
										Context->ListerCursor = Context->ListerCount + Direction;
									} else {
										Context->ListerCursor = (Context->ListerCursor + Direction) % Context->ListerCount;
									}
								} break;
							}
							
							Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key); 
						} break;
						case VimMode_Insert: {
							if (LatestKey->Input == PloreKey_Backspace) {
								if (Context->CommandKeyCount > 1) {
									Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key);
								}
								Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key);
							} 
							
						} break;
						
					}
					Result.Command.Type = Context->ActiveCommand.Type;
					Result.Command.State = VimCommandState_Incomplete;
					Context->ActiveCommand = Result.Command; // @Cleanup
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

PLORE_VIM_COMMAND(OpenFile) {
	plore_file *Selected = GetCursorFile(State);
	if (Selected->Type != PloreFileNode_File) return; // TODO(Evan): Multiple tabs?!
	
	if (Command.Shell) {
		if (Selected) {
			Platform->RunShell(Command.Shell, Selected->Path.Absolute);
		}
		ResetVimState(VimContext);
		
	} else {
		switch (Command.State) {
			case VimCommandState_Start: {
				u64 MaybeCount = GetHandlerCount(Selected->Extension);
				if (MaybeCount) {
					VimContext->ListerCount = MaybeCount;
					VimContext->ListerCursor = 0;
					SetActiveCommand(VimContext, Command);
					VimContext->Mode = VimMode_Lister;
				} else {
					DrawText("<suggestion list>");
					ResetVimState(VimContext);
				}
			} break;
			
			case VimCommandState_Finish: {
				Assert(VimContext->ListerCursor < GetHandlerCount(Selected->Extension));
				
				plore_handler *Handler = PloreFileExtensionHandlers[Selected->Extension] + VimContext->ListerCursor;
				Platform->RunShell(Handler->Shell, Selected->Path.Absolute);
			} break;
		}
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
		if (!Tab->DirectoryState->Current.Count) break;
		plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(FileContext, &Tab->DirectoryState->Current.File.Path);
		plore_file *CursorEntry = Tab->DirectoryState->Current.Entries + CursorResult.Cursor->Cursor;
		
		if (CursorEntry->Type == PloreFileNode_Directory) {
			PrintLine("Moving down a directory, from %s to %s", 
					  Tab->DirectoryState->Current.File.Path.Absolute, 
					  CursorEntry->Path.Absolute);
			Platform->SetCurrentDirectory(CursorEntry->Path.Absolute);
			
			ScalarCount++;
			
			if (Platform->IsPathTopLevel(CursorEntry->Path.Absolute, PLORE_MAX_PATH)) return;
			else SynchronizeCurrentDirectory(GetCurrentTab(State));
		} else if (!WasGivenScalar) {
			if (CursorEntry->Extension) {
				u64 HandlerCount = GetHandlerCount(CursorEntry->Extension);
				
				if (HandlerCount >= 1) {
					char *DefaultHandler = PloreFileExtensionHandlers[CursorEntry->Extension][0].Shell;
					vim_command OpenTheFile = {
						.Type = VimCommandType_OpenFile,
						.Shell = DefaultHandler
					};
					SetActiveCommand(VimContext, OpenTheFile);
					DoOpenFile(State, GetCurrentTab(State), VimContext, FileContext, OpenTheFile);
				} 
			} else {
				if (Tab->DirectoryState->Current.Valid) {
					DrawText("Unknown extension for `%s`", CursorEntry->Path.FilePart);
				}
			}
		}
		
		if (!Tab->DirectoryState->Current.Count) return;
		
	}
	Command.Scalar = ScalarCount;
}

internal void
MoveHelper(plore_state *State, vim_command Command, i64 Direction) {
	plore_tab *Tab = GetCurrentTab(State);
	plore_file_context *FileContext = Tab->FileContext;
	if (Tab->DirectoryState->Current.Count) {
		plore_file_listing_cursor_get_or_create_result Current = GetOrCreateCursor(FileContext, &Tab->DirectoryState->Current.File.Path);
		u64 CursorStart = Current.Cursor->Cursor;
		
		u64 Magnitude = Command.Scalar % Tab->DirectoryState->Current.Count;
		
		while (Magnitude) {
			if (Direction == -1) {
				if (Current.Cursor->Cursor == 0) {
					Current.Cursor->Cursor = Tab->DirectoryState->Current.Count-1;
				} else {
					Current.Cursor->Cursor = (Current.Cursor->Cursor-1) % Tab->DirectoryState->Current.Count;
				}
			} else if (Direction == +1) {
				Current.Cursor->Cursor = (Current.Cursor->Cursor + 1) % Tab->DirectoryState->Current.Count;
			}
			
			plore_file *NewSelected = Tab->DirectoryState->Current.Entries + Current.Cursor->Cursor;
			if (!Tab->FilterState->ShowHidden) {
				if (NewSelected->Hidden) {
					if (Current.Cursor->Cursor == CursorStart) break;
					else                                       continue;
				}
			}
			Magnitude--;
		}
		
	}
}

PLORE_VIM_COMMAND(MoveUp) {
	MoveHelper(State, Command, -1);
}

PLORE_VIM_COMMAND(MoveDown)  {
	MoveHelper(State, Command, +1);
}

PLORE_VIM_COMMAND(Yank)  {
	if (FileContext->SelectedCount > 1) { // Yank selection if there is any.
		MemoryCopy(FileContext->Selected, FileContext->Yanked, sizeof(FileContext->Yanked));
		FileContext->YankedCount = FileContext->SelectedCount;
	} else { 
		if (Command.Scalar > 1) {
			plore_file_listing *CurrentDirectory = &Tab->DirectoryState->Current;
			if (CurrentDirectory->Count) {
				plore_file_listing_cursor_get_or_create_result CurrentResult = GetOrCreateCursor(FileContext, &Tab->DirectoryState->Current.File.Path);
				u64 Cursor = CurrentResult.Cursor->Cursor;
				while (Command.Scalar--) {
					ToggleYanked(FileContext, &CurrentDirectory->Entries[Cursor].Path);
					Cursor = (Cursor + 1) % CurrentDirectory->Count;
				}
			}
		} else {
			plore_path *Selected = GetImpliedSelection(State);
			if (Selected) ToggleYanked(FileContext, Selected);
		}
	}
	
}

PLORE_VIM_COMMAND(ClearYank)  {
	if (FileContext->YankedCount) {
		FileContext->YankedCount = 0;
		FileContext->SelectedCount = 0;
	}
}

PLORE_VIM_COMMAND(Paste)  {
	if (FileContext->YankedCount) { // @Cleanup
		u64 PastedCount = 0;
		plore_file_listing *Current = &Tab->DirectoryState->Current;
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
		
		FileContext->YankedCount = 0;
		MemoryClear(FileContext->Yanked, sizeof(FileContext->Yanked));
		
		FileContext->SelectedCount = 0;
		MemoryClear(FileContext->Selected, sizeof(FileContext->Selected));
	}
}

internal void DoSelectHelper(plore_state *State, vim_command Command, i64 Direction) {
	plore_tab *Tab = GetCurrentTab(State);
	plore_file_context *FileContext = Tab->FileContext;
	plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(FileContext, &Tab->DirectoryState->Current.File.Path);
	plore_file *Selectee = Tab->DirectoryState->Current.Entries + CursorResult.Cursor->Cursor;
	u64 ScalarCount = 0;
	u64 StartScalar = CursorResult.Cursor->Cursor;
	while (Command.Scalar--) {
		ToggleSelected(FileContext, &Tab->DirectoryState->Cursor.File.Path);
		if (Direction == +1) {
			CursorResult.Cursor->Cursor = (CursorResult.Cursor->Cursor + 1) % Tab->DirectoryState->Current.Count;
		} else {
			if (CursorResult.Cursor->Cursor == 0) {
				CursorResult.Cursor->Cursor = Tab->DirectoryState->Current.Count - 1;
			} else {
				CursorResult.Cursor->Cursor -= 1;
			}
		}
		Selectee = Tab->DirectoryState->Current.Entries + CursorResult.Cursor->Cursor;
		
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
	plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(FileContext, &Tab->DirectoryState->Current.File.Path);
	CursorResult.Cursor->Cursor = 0;
}

PLORE_VIM_COMMAND(JumpBottom)  {
	plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(FileContext, &Tab->DirectoryState->Current.File.Path);
	if (Tab->DirectoryState->Current.Count) {
		CursorResult.Cursor->Cursor = Tab->DirectoryState->Current.Count-1;
	}
}

PLORE_VIM_COMMAND(ISearch)  {
	if (Command.Shell) {
		if (Tab->DirectoryState->Current.Count) {
			for (u64 F = 0; F < Tab->DirectoryState->Current.Count; F++) {
				plore_file *File = Tab->DirectoryState->Current.Entries + F;
				
				// TODO(Evan): Filter highlighting.
				if (SubstringNoCase(File->Path.FilePart, Tab->FilterState->TextFilter).IsContained) {
					plore_file_listing_cursor *CurrentCursor = GetCursor(FileContext, Tab->DirectoryState->Current.File.Path.Absolute);
					CurrentCursor->Cursor = F;
					MemoryClear(Tab->FilterState->TextFilter, ArrayCount(Tab->FilterState->TextFilter));
					break;
				}
			}
			
			Tab->FilterState->TextFilterCount = 0;
		}
	} else {
		switch (Command.State) {
			case VimCommandState_Start: {
				ClearCommands(VimContext);
				InsertBegin(VimContext, Command);
			} break;
			
			case VimCommandState_Incomplete: {
				Tab->FilterState->TextFilterCount = VimKeysToString(Tab->FilterState->TextFilter, 
																	 ArrayCount(Tab->FilterState->TextFilter), 
																	 VimContext->CommandKeys).BytesWritten;
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
				InsertBegin(VimContext, Command);
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
			
			InsertBegin(VimContext, Command);
		} break;
		
		case VimCommandState_Incomplete: {
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
	plore_file_listing_cursor_get_or_create_result CursorResult = GetOrCreateCursor(FileContext, &Tab->DirectoryState->Current.File.Path);
	plore_file *Selectee = Tab->DirectoryState->Current.Entries + CursorResult.Cursor->Cursor;
	ToggleSelected(FileContext, &Tab->DirectoryState->Cursor.File.Path);
}

PLORE_VIM_COMMAND(NewTab) {
	if (State->TabCount == ArrayCount(State->Tabs)) return;
	
	if (Command.Pattern[0]) {
		i32 NewTab = StringToI32(Command.Pattern);
		if (NewTab <= 0) return;
		NewTab -= 1;
		SetCurrentTab(State, NewTab);
	}
}

PLORE_VIM_COMMAND(CloseTab) {
	if (State->TabCount > 1) {
		u64 TabIndex = 0xdeadbeef;
		for (u64 T = 0; T < State->TabCount; T++) {
			plore_tab *Other = State->Tabs + T;
			if (Tab == Other) {
				TabIndex = T;
				break;
			}
		}
		
		Assert(TabIndex < ArrayCount(State->Tabs));
		Assert(TabIndex < State->TabCount);
		
		u64 tBefore = 0;
		u64 tAfter = State->TabCount;
		for (u64 T = 0; T < State->TabCount; T++) {
			plore_tab *Other = State->Tabs + T;
			if (Other == Tab) continue;
			
			if (Other->Active) {
				if (T < TabIndex) tBefore = T;
				else              tAfter = T;
				
				break;
			}
		}
		
		u64 tTarget = 0;
		if (AbsDifference(TabIndex, tBefore) > AbsDifference(TabIndex, tAfter)) {
			tTarget = tAfter;
		} else {
			tTarget = tBefore;
		}
		
		ClearTab(State, TabIndex);
		SetCurrentTab(State, tTarget);
		
	}
}

PLORE_VIM_COMMAND(OpenShell) {
	Platform->RunShell(Command.Shell, "");
}

PLORE_VIM_COMMAND(CreateFile) {
	if (Command.Shell) {
		char *FilePath = PushBytes(&State->FrameArena, PLORE_MAX_PATH);
		u64 BytesWritten = CStringCopy(Tab->DirectoryState->Current.File.Path.Absolute, FilePath, PLORE_MAX_PATH);
		FilePath[BytesWritten++] = '\\';
		CStringCopy(Command.Shell, FilePath+BytesWritten, PLORE_MAX_PATH);
		PrintLine("%s", FilePath);
		
		// TODO(Evan): Validate file name.
		Platform->CreateFile(FilePath, false);
		
	} else {
		switch (Command.State) {
			case VimCommandState_Start: {
				ClearCommands(VimContext);
				InsertBegin(VimContext, Command);
			} break;
		}
	}
}

PLORE_VIM_COMMAND(CreateDirectory) {
	if (Command.Shell) {
		Platform->CreateDirectory(Command.Shell);
	} else {
		switch (Command.State) {
			case VimCommandState_Start: {
				ClearCommands(VimContext);
				InsertBegin(VimContext, Command);
			} break;
		}
	}
}

PLORE_VIM_COMMAND(DeleteFile) {
	if (Command.Shell) {
		u64 Size = 64;
		char *Buffer = PushBytes(&State->FrameArena, Size);
		char *MaybeConfirmation = VimKeysToString(Buffer, Size, VimContext->CommandKeys).Buffer;
		
		if (Confirmation(MaybeConfirmation)) {
			plore_path *Selection = GetImpliedSelection(State);
			if (Selection) {
				b64 Result = Platform->DeleteFile(Selection->Absolute, (platform_delete_file_desc) { 
													  .Recursive = false,
													  .PermanentDelete = false,
												  });
			}
		}
	} else {
		switch (Command.State) {
			case VimCommandState_Start: {
				ClearCommands(VimContext);
				InsertBegin(VimContext, Command);
			} break;
		}
	}
}

PLORE_VIM_COMMAND(YankAll) {
	plore_file_listing *CurrentDirectory = &Tab->DirectoryState->Current;
	for (u64 F = 0; F < CurrentDirectory->Count; F++) {
		plore_file *File = CurrentDirectory->Entries + F;
		if (!IsYanked(FileContext, &File->Path)) {
			ToggleYanked(FileContext, &File->Path);
		}
	}
}

PLORE_VIM_COMMAND(SelectAll) {
	plore_file_listing *CurrentDirectory = &Tab->DirectoryState->Current;
	for (u64 F = 0; F < CurrentDirectory->Count; F++) {
		plore_file *File = CurrentDirectory->Entries + F;
		if (!IsSelected(FileContext, &File->Path)) {
			ToggleSelected(FileContext, &File->Path);
		}
	}
}

PLORE_VIM_COMMAND(ShowHiddenFiles) {
	Tab->FilterState->ShowHidden = !Tab->FilterState->ShowHidden;
	if (!Tab->FilterState->ShowHidden) {
		plore_file *Cursor = GetCursorFile(State);
		if (Cursor->Hidden) {
			DrawText("Cursor is hidden now!");
		}
	}
}

PLORE_VIM_COMMAND(ToggleSortName) {
	State->FilterState.SortAscending[FileSort_Name] = !State->FilterState.SortAscending[FileSort_Name];
}

PLORE_VIM_COMMAND(ToggleSortSize) {
	State->FilterState.SortAscending[FileSort_Size] = !State->FilterState.SortAscending[FileSort_Size];
}

PLORE_VIM_COMMAND(ToggleSortModified) {
	State->FilterState.SortAscending[FileSort_Modified] = !State->FilterState.SortAscending[FileSort_Modified];
}


#define PLORE_X(Name, Ignored1, _Ignored2, _Ignored3) Do##Name,
vim_command_function *VimCommands[] = {
	VIM_COMMANDS
};
#undef PLORE_X


