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

internal void
SetVimCommandFromString(plore_vim_context *Context, char *S) {
	string_to_command_result Result = StringToCommand(S);
	MemoryCopy(Result.CommandKeys, Context->CommandKeys, sizeof(Context->CommandKeys));
	Context->CommandKeyCount = Result.CommandKeyCount;
}

internal void
InsertBegin(plore_vim_context *VimContext, vim_command Command) {
	VimContext->Mode = VimMode_Insert;
	SetActiveCommand(VimContext, Command);
}

internal b64
Confirmation(char *S) {
	b64 Result = false;
	Result = StringsAreEqualIgnoreCase(S, "y")   ||
		     StringsAreEqualIgnoreCase(S, "yy")  ||
		     StringsAreEqualIgnoreCase(S, "yes");

	return(Result);
}

internal void
ResetListerState(plore_vim_context *Context) {
	Context->ListerState.Mode = VimListerMode_Normal;
	Context->ListerState.HideFiles = 0;
	Context->ListerState.Cursor = 0;
	Context->ListerState.Count = 0;
}

internal void
ResetVimState(plore_vim_context *Context) {
	ClearCommands(Context);
	Context->Mode = VimMode_Normal;
	Context->ActiveCommand = ClearStruct(vim_command);
	ResetListerState(Context);
}


plore_inline vim_key *
PeekLatestKey(plore_vim_context *Context) {
	vim_key *Result = 0;
	if (Context->CommandKeyCount) {
		Result = Context->CommandKeys + Context->CommandKeyCount-1;
	}

	return(Result);
}


plore_inline b64
RequireBackspace(vim_key *Key) {
	b64 Result = Key->Input == PloreKey_Backspace;
	return(Result);
}

internal void
DoBackSpace(plore_vim_context *Context) {
	if (Context->CommandKeyCount > 1) {
		Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key);
	}
	Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key);
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

			if (C->Input == PloreKey_Return && Context->CommandKeyCount > 1) return(Result);

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

						b64 StraightMatch = StructArrayMatch(VimBindings[Candidate].Keys, C, NonScalarEntries);

						b64 PatternMatch = false;

						if (AcceptsPattern(VimBindings + Candidate)) {
							for (u64 K = 0; K < ArrayCount(VimBindings[Candidate].Keys); K++) {
								vim_key *Key = VimBindings[Candidate].Keys + K;

								if (Key->Pattern) {
									if (Key->Modifier != C[K].Modifier || Key->Pattern != C[K].Pattern) {
										PatternMatch = false;
										break;
									} else {
										PatternMatch = true;
										Assert(Result.Command.PatternCount < ArrayCount(Result.Command.Pattern) - 1);
										Result.Command.Pattern[Result.Command.PatternCount++] = PloreKeyCharacters[C[K].Input];
									}
								} else {
									if (MemoryCompare(C+K, Key, sizeof(vim_key)) != 0) {
										PatternMatch = false;
										break;
									} else {
										PatternMatch = true;
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
					if (RequireBackspace(LatestKey)) DoBackSpace(Context);
					else                             Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key);

					Result.Command.State = VimCommandState_Incomplete;

					// NOTE(Evan): Don't steal return from lister.
				} else if (LatestKey->Input == PloreKey_Return && Context->ListerState.Mode != VimListerMode_ISearch) {
					Result.Command.Type = Context->ActiveCommand.Type;
					Result.Command.State = VimCommandState_Finish;
					Context->ActiveCommand = Result.Command; // @Cleanup

					Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key);
				} else {
					Result.Command.State = VimCommandState_Incomplete;

					// NOTE(Evan): Mode-specific keymapping is here, as Return, Escape and a full buffer terminate always.
					switch (Context->Mode) {
						case VimMode_Lister: {
							switch (Context->ListerState.Mode) {
								case VimListerMode_Normal: {
									switch (LatestKey->Input) {
										case PloreKey_Escape: {
											ResetVimState(Context);
											Result.Command.State = VimCommandState_Finish;
											Result.Command.Type = VimCommandType_None;
											return(Result);
										} break;
										case PloreKey_J:
										case PloreKey_K: {
											i64 Direction = LatestKey->Input == PloreKey_J ? +1 : -1;
											if (Direction < 0 && Context->ListerState.Cursor == 0) {
												Context->ListerState.Cursor = Context->ListerState.Count + Direction;
											} else {
												Context->ListerState.Cursor = (Context->ListerState.Cursor + Direction) % Context->ListerState.Count;
											}
										} break;
										// NOTE(Evan): Lister empties the command buffer; we may want to preserve this between Lister ISearches.
										case PloreKey_Slash: {
											Context->ListerState.Mode = VimListerMode_ISearch;
											ClearCommands(Context);
										} break;
									}

									if (Context->CommandKeyCount) Context->CommandKeys[--Context->CommandKeyCount] = ClearStruct(vim_key);
								} break;

								case VimListerMode_ISearch: {
									switch (LatestKey->Input) {
										case PloreKey_Backspace: {
											DoBackSpace(Context);
										} break;

										case PloreKey_Escape: {
											Context->ListerState.Mode = VimListerMode_Normal;
										} break;

										case PloreKey_Return: {
											Context->ListerState.Mode = VimListerMode_Normal;
											ClearCommands(Context);
										} break;

										default: {
											char Filter[256] = {0};
											VimKeysToString(Filter, ArrayCount(Filter), Context->CommandKeys).Buffer;

											for (u64 L = 0; L < Context->ListerState.Count; L++) {
												u64 Current = (Context->ListerState.Cursor + L) % Context->ListerState.Count;
												char *Title = Context->ListerState.Titles[Current];
												if (SubstringNoCase(Title, Filter).IsContained) {
													Context->ListerState.Cursor = Current;
													break;
												}
											}
										} break;
									}
								} break;

								InvalidDefaultCase;
							} break;

						} break;


						case VimMode_Insert: {
							switch (LatestKey->Input) {
								case PloreKey_Backspace: {
									DoBackSpace(Context);
								} break;

								case PloreKey_Escape: {
									Result.Command.State = VimCommandState_Cancel;
								} break;
							}
						} break;

					}
					Result.Command.Type = Context->ActiveCommand.Type;
					Context->ActiveCommand = Result.Command; // @Cleanup
				}
			}

		} break;

		InvalidDefaultCase;
	}

	return(Result);
}

// NOTE(Evan): Vim command prototype and function table generation.

typedef struct plore_tab plore_tab;

#define PLORE_VIM_COMMAND_FUNCTION(Name) \
void Name(plore_state *State, plore_tab *Tab, plore_vim_context *VimContext, plore_file_context *FileContext, vim_command Command)

#define PLORE_DO_VIM_COMMAND(Name) Do##Name(State, Tab, VimContext, FileContext, Command)
#define PLORE_DO_VIM_COMMAND_ENUM(Enum) VimCommands[Enum](State, Tab, VimContext, FileContext, Command)

typedef PLORE_VIM_COMMAND_FUNCTION(vim_command_function);
#define PLORE_VIM_COMMAND(CommandName) PLORE_VIM_COMMAND_FUNCTION(Do##CommandName)

// NOTE(Evan): ListerEnd() and ShowCommandList use the dispatch table directly, so declare it here.
extern vim_command_function *VimCommands[VimCommandType_Count];

typedef struct vim_lister_begin_desc {
	vim_command Command;
	char **Titles;
	char **Secondaries;
	u64 Count;
	b64 HideFiles;
} vim_lister_begin_desc;
internal void
ListerBegin(plore_vim_context *VimContext, vim_lister_begin_desc Desc) {
	Assert(!VimContext->ListerState.Count);
	Assert(Desc.Titles || Desc.Secondaries);

	VimContext->Mode = VimMode_Lister;
	SetActiveCommand(VimContext, Desc.Command);
	ClearCommands(VimContext);

	if (Desc.Titles) {
		u64 BytesCopied = 0;
		for (u64 L = 0; L < Desc.Count; L++) {
			StringCopy(Desc.Titles[L],
					   VimContext->ListerState.Titles[L],
					   ArrayCount(VimContext->ListerState.Titles[L]));
		}
	}
	if (Desc.Secondaries) {
		for (u64 L = 0; L < Desc.Count; L++) {
			StringCopy(Desc.Secondaries[L],
					   VimContext->ListerState.Secondaries[L],
					   ArrayCount(VimContext->ListerState.Secondaries[L]));
		}
	}

	VimContext->ListerState.Count = Desc.Count;
	VimContext->ListerState.HideFiles = Desc.HideFiles;
	VimContext->ListerState.Cursor = 0;
	VimContext->ListerState.Mode = 0;
}

internal void
ListerEnd(plore_state *State, u64 Listee) {
	plore_vim_context *VimContext = State->VimContext;

	Assert(VimContext->Mode == VimMode_Lister);
	Assert(Listee < VimContext->ListerState.Count);

	VimContext->ListerState.Cursor = Listee;
	vim_command Command = {
		.Type = VimContext->ActiveCommand.Type,
		.State = VimCommandState_Finish,
	};
	VimCommands[Command.Type](State, GetCurrentTab(State), VimContext, GetCurrentTab(State)->FileContext, Command);
}



PLORE_VIM_COMMAND(None) {
	Assert(!"None command function reached!");
}

PLORE_VIM_COMMAND(OpenFileWith) {
	if (Command.Shell) {
		plore_file *Target = GetCursorFile(State);
		if (Target) {
			Platform->RunShell((platform_run_shell_desc) {
								   .Command = Command.Shell,
								   .Args = Target->Path.Absolute,
								   .QuoteArgs = true,
							   });
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

PLORE_VIM_COMMAND(OpenFile) {
	plore_file *Selected = GetCursorFile(State);
	if (Selected->Type != PloreFileNode_File) return;

	b64 MaybeQuoteArgs = true;

	if (Command.Shell) {
		// NOTE(Evan): Windows photo viewer does not allow the argument to be quoted.
#if defined(PLORE_WINDOWS)
		if (StringsAreEqual(Command.Shell, PLORE_PHOTO_VIEWER)) MaybeQuoteArgs = false;
#endif

		if (Selected) {
			Platform->RunShell((platform_run_shell_desc) {
								   .Command = Command.Shell,
								   .Args = Selected->Path.Absolute,
								   .QuoteArgs = MaybeQuoteArgs,
							   });
		}
		ResetVimState(VimContext);

	} else {
		switch (Command.State) {
			case VimCommandState_Start: {
				u64 MaybeCount = GetHandlerCount(Selected->Extension);
				if (MaybeCount) {

					// NOTE(Evan): We copy the handler strings out because there's not an easy way to specify n-tuples withinin the PLORE_X macro
					// that defines struct literals.
					// Yet another reason for a small metaprogram!
					char *TitleBuffer[VimListing_ListSize] = {0};
					for (u64 L = 0; L < MaybeCount; L++) {
						TitleBuffer[L] = PushBytes(&State->FrameArena, VimListing_Size);
						StringCopy(PloreFileExtensionHandlers[Selected->Extension][L].Name, TitleBuffer[L], VimListing_Size);
					}

					char *SecondaryBuffer[VimListing_ListSize] = {0};
					for (u64 L = 0; L < MaybeCount; L++) {
						SecondaryBuffer[L] = PushBytes(&State->FrameArena, VimListing_Size);
						StringCopy(PloreFileExtensionHandlers[Selected->Extension][L].Shell, SecondaryBuffer[L], VimListing_Size);
					}

					ListerBegin(VimContext, (vim_lister_begin_desc) {
									.Command = Command,
									.Titles = TitleBuffer,
									.Secondaries = SecondaryBuffer,
									.Count = MaybeCount,
								});
				} else {
					Command = (vim_command) {
						.Type = VimCommandType_OpenFileWith,
						.State = VimCommandState_Start,
					};
					PLORE_DO_VIM_COMMAND(OpenFileWith);
				}
			} break;

			case VimCommandState_Finish: {
				Assert(VimContext->ListerState.Cursor < GetHandlerCount(Selected->Extension));

				plore_handler *Handler = PloreFileExtensionHandlers[Selected->Extension] + VimContext->ListerState.Cursor;

				// NOTE(Evan): Windows photo viewer does not allow the argument to be quoted.
#if defined(PLORE_WINDOWS)
				if (StringsAreEqual(Handler->Shell, PLORE_PHOTO_VIEWER)) MaybeQuoteArgs = false;
#endif

				Platform->RunShell((platform_run_shell_desc) {
									   .Command = Handler->Shell,
									   .Args = Selected->Path.Absolute,
									   .QuoteArgs = MaybeQuoteArgs,
								   });
			} break;
		}
	}
}
PLORE_VIM_COMMAND(MoveLeft) {
	u64 ScalarCount = 0;
	while (Command.Scalar--) {
		plore_path Buffer = Platform->GetCurrentDirectoryPath();
		char *Path = Buffer.Absolute;

		Platform->PathPop(Path, PLORE_MAX_PATH, false);

		Platform->SetCurrentDirectory(Path);
		ScalarCount++;

		if (Platform->PathIsTopLevel(Path)) break;
	}
	Command.Scalar = ScalarCount;
}

PLORE_VIM_COMMAND(MoveRight) {
	u64 WasGivenScalar = Command.Scalar > 1;
	u64 ScalarCount = 0;
	while (Command.Scalar--) {
		if (!Tab->DirectoryState->Current.Count) break;
		plore_file_listing_info_get_or_create_result CursorResult = GetOrCreateFileInfo(FileContext, &Tab->DirectoryState->Current.File.Path);
		plore_file *CursorEntry = Tab->DirectoryState->Current.Entries + CursorResult.Info->Cursor;

		if (CursorEntry->Type == PloreFileNode_Directory) {
			Platform->SetCurrentDirectory(CursorEntry->Path.Absolute);

			ScalarCount++;

			if (Platform->PathIsTopLevel(CursorEntry->Path.Absolute)) return;
			else SynchronizeCurrentDirectory(&State->FrameArena, GetCurrentTab(State));
		} else if (!WasGivenScalar) {
			if (CursorEntry->Extension) {
				u64 HandlerCount = GetHandlerCount(CursorEntry->Extension);

				if (HandlerCount >= 1) {
					char *DefaultHandler = PloreFileExtensionHandlers[CursorEntry->Extension][0].Shell;
					Command = (vim_command) {
						.Type = VimCommandType_OpenFile,
						.Shell = DefaultHandler
					};
					PLORE_DO_VIM_COMMAND(OpenFile);
				}
			} else {
				if (Tab->DirectoryState->Current.Valid) {
					PrintLine("Unknown extension for `%s`", CursorEntry->Path.FilePart);
				}
			}
		}

		if (!Tab->DirectoryState->Current.Count) return;

	}
}

internal void
MoveHelper(plore_state *State, vim_command Command, i64 Direction) {
	plore_tab *Tab = GetCurrentTab(State);
	plore_file_context *FileContext = Tab->FileContext;
	if (Tab->DirectoryState->Current.Count) {
		DrawText("Here");
		plore_file_listing_info_get_or_create_result Current = GetOrCreateFileInfo(FileContext, &Tab->DirectoryState->Current.File.Path);
		u64 CursorStart = Current.Info->Cursor;

		u64 Magnitude = Command.Scalar % Tab->DirectoryState->Current.Count;

		while (Magnitude) {
			if (Direction == -1) {
				if (Current.Info->Cursor == 0) {
					Current.Info->Cursor = Tab->DirectoryState->Current.Count-1;
				} else {
					Current.Info->Cursor = (Current.Info->Cursor-1) % Tab->DirectoryState->Current.Count;
				}
			} else if (Direction == +1) {
				Current.Info->Cursor = (Current.Info->Cursor + 1) % Tab->DirectoryState->Current.Count;
			}

			plore_file *NewSelected = Tab->DirectoryState->Current.Entries + Current.Info->Cursor;
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

PLORE_VIM_COMMAND(PageDown) {
	plore_file_listing_info *Info = GetOrCreateFileInfo(FileContext, &Tab->DirectoryState->Current.File.Path).Info;
	u64 NewCursor = Info->Cursor + PAGE_MOVE_DISTANCE;
	if (NewCursor < Tab->DirectoryState->Current.Count) {
		Info->Cursor = NewCursor;
	} else {
		Info->Cursor = Tab->DirectoryState->Current.Count-1;
	}
}

PLORE_VIM_COMMAND(PageUp) {
	plore_file_listing_info *Info = GetOrCreateFileInfo(FileContext, &Tab->DirectoryState->Current.File.Path).Info;
	u64 NewCursor = Info->Cursor - PAGE_MOVE_DISTANCE;
	if (Info->Cursor >= PAGE_MOVE_DISTANCE) {
		Info->Cursor = NewCursor;
	} else {
		Info->Cursor = 0;
	}
}

PLORE_VIM_COMMAND(Yank)  {
	if (MapCount(FileContext->Selected)) { // Yank selection if there is any.

		ForMap(FileContext->Selected, file_lookup) {
			MapInsert(State->Yanked, It);
		}

		MapReset(FileContext->Selected);
	} else {
		if (Command.Scalar > 1) {
			plore_file_listing *CurrentDirectory = &Tab->DirectoryState->Current;
			if (CurrentDirectory->Count) {
				plore_file_listing_info_get_or_create_result CurrentResult = GetOrCreateFileInfo(FileContext, &Tab->DirectoryState->Current.File.Path);
				u64 Cursor = CurrentResult.Info->Cursor;
				while (Command.Scalar--) {
					ToggleYanked(State->Yanked, &CurrentDirectory->Entries[Cursor].Path);
					Cursor = (Cursor + 1) % CurrentDirectory->Count;
				}
			}
		} else {
			plore_path *Selected = GetImpliedSelectionPath(State);
			if (Selected) ToggleYanked(State->Yanked, Selected);
		}
	}

}

PLORE_VIM_COMMAND(ClearYank)  {
	if (MapCount(State->Yanked)) MapReset(State->Yanked);
}

PLORE_VIM_COMMAND(Paste)  {
	if (MapCount(State->Yanked)) { // @Cleanup
		InvalidCodePath;
		u64 PastedCount = 0;
		char *NewDirectory = Tab->DirectoryState->Current.File.Path.Absolute;

		task_with_memory_handle TaskHandle = Platform->CreateTaskWithMemory(State->Taskmaster);
		plore_move_files_header *Header = PushStruct(TaskHandle.Arena, plore_move_files_header);
		Header->sAbsolutePaths = PushBytes(TaskHandle.Arena, sizeof(char *)*MapCount(State->Yanked));
		Header->dAbsolutePaths = PushBytes(TaskHandle.Arena, sizeof(char *)*MapCount(State->Yanked));
		for (u64 P = 0; P < MapCount(State->Yanked); P++) {
			Header->sAbsolutePaths[P] = PushBytes(TaskHandle.Arena, PLORE_MAX_PATH);
			Header->dAbsolutePaths[P] = PushBytes(TaskHandle.Arena, PLORE_MAX_PATH);
		}

		Header->Count = 0;
		ForMap(State->Yanked, file_lookup) {
			PathCopy(It->K, Header->sAbsolutePaths[Header->Count]);
			char *NewPath = Header->dAbsolutePaths[Header->Count];
			Platform->PathJoin(NewPath, NewDirectory, It->FilePart);
			Header->Count += 1;
		}

		Platform->StartTaskWithMemory(State->Taskmaster, TaskHandle, MoveFiles, Header);

		MapReset(State->Yanked);
		MapReset(FileContext->Selected);
	}
}

internal void DoSelectHelper(plore_state *State, vim_command Command, i64 Direction) {
	plore_tab *Tab = GetCurrentTab(State);
	plore_file_context *FileContext = Tab->FileContext;
	plore_file_listing_info_get_or_create_result CursorResult = GetOrCreateFileInfo(FileContext, &Tab->DirectoryState->Current.File.Path);
	plore_file *Selectee = Tab->DirectoryState->Current.Entries + CursorResult.Info->Cursor;
	u64 ScalarCount = 0;
	u64 StartScalar = CursorResult.Info->Cursor;
	while (Command.Scalar--) {
		ToggleSelected(Tab->FileContext->Selected, &Tab->DirectoryState->Cursor.File.Path);
		if (Direction == +1) {
			CursorResult.Info->Cursor = (CursorResult.Info->Cursor + 1) % Tab->DirectoryState->Current.Count;
		} else {
			if (CursorResult.Info->Cursor == 0) {
				CursorResult.Info->Cursor = Tab->DirectoryState->Current.Count - 1;
			} else {
				CursorResult.Info->Cursor -= 1;
			}
		}
		Selectee = Tab->DirectoryState->Current.Entries + CursorResult.Info->Cursor;

		ScalarCount++;
		if (CursorResult.Info->Cursor == StartScalar) break;
	}
}

PLORE_VIM_COMMAND(SelectUp)  {
	DoSelectHelper(State, Command, -1);
}

PLORE_VIM_COMMAND(SelectDown)  {
	DoSelectHelper(State, Command, +1);
}
PLORE_VIM_COMMAND(JumpTop)  {
	plore_file_listing_info_get_or_create_result CursorResult = GetOrCreateFileInfo(FileContext, &Tab->DirectoryState->Current.File.Path);
	CursorResult.Info->Cursor = 0;
}

PLORE_VIM_COMMAND(JumpBottom)  {
	plore_file_listing_info_get_or_create_result CursorResult = GetOrCreateFileInfo(FileContext, &Tab->DirectoryState->Current.File.Path);
	if (Tab->DirectoryState->Current.Count) {
		CursorResult.Info->Cursor = Tab->DirectoryState->Current.Count-1;
	}
}

PLORE_VIM_COMMAND(ISearch)  {
	switch (Command.State) {
		case VimCommandState_Start: {
			ClearCommands(VimContext);
			InsertBegin(VimContext, Command);
		} break;

		case VimCommandState_Incomplete: {
			// @Cleanup, this is really only used for highlighting.
			Tab->FilterState->ISearchFilter.TextCount = VimKeysToString(Tab->FilterState->ISearchFilter.Text,
																        ArrayCount(Tab->FilterState->ISearchFilter.Text),
																        VimContext->CommandKeys).BytesWritten;

			if (Tab->DirectoryState->Current.Count) {
				for (u64 F = 0; F < Tab->DirectoryState->Current.Count; F++) {
					plore_file_listing_info *CurrentCursor = &MapGet(FileContext->FileInfo, &Tab->DirectoryState->Current.File.Path)->V;
					u64 Current = (CurrentCursor->Cursor + F) % Tab->DirectoryState->Current.Count;
					plore_file *File = Tab->DirectoryState->Current.Entries + Current;

					if (SubstringNoCase(File->Path.FilePart, Tab->FilterState->ISearchFilter.Text).IsContained) {
						plore_file_listing_info *CurrentCursor = &MapGet(FileContext->FileInfo, &Tab->DirectoryState->Current.File.Path)->V;
						CurrentCursor->Cursor = Current;
						break;
					}
				}
			}

		} break;

		case VimCommandState_Cancel: {
			Tab->FilterState->ISearchFilter.TextCount = 0;
		} break;

		// NOTE(Evan): Only do movement/opening if there is an isearch filter and the cursor is a match for the it.
		case VimCommandState_Finish: {
			if (Tab->FilterState->ISearchFilter.TextCount) {
				if (Substring(Tab->FilterState->ISearchFilter.Text, GetCursorFile(State)->Path.FilePart).IsContained) {
					PLORE_DO_VIM_COMMAND(MoveRight);
				}
				Tab->FilterState->ISearchFilter.TextCount = 0;
			}
		} break;
	}
}

internal void
TextFilterHelper(plore_vim_context *VimContext,
				 plore_tab *Tab,
				 vim_command Command,
				 text_filter_state TargetState,
				 b64 Global) {
	text_filter *Filter = 0;
	if (Global) {
		Filter = &Tab->FilterState->GlobalListingFilter;
	} else {
		plore_file_listing_info *Info = GetOrCreateFileInfo(Tab->FileContext, &Tab->CurrentDirectory).Info;
		Filter = &Info->Filter;
	}

	switch (Command.State) {
		case VimCommandState_Start: {
			InsertBegin(VimContext, Command);
			if (Filter->State != TargetState) *Filter = ClearStruct(text_filter);
			SetVimCommandFromString(VimContext, Filter->Text);

			Filter->State = TargetState;
		} break;

		case VimCommandState_Incomplete: {
			Filter->TextCount = VimKeysToString(Filter->Text,
											    ArrayCount(Filter->Text),
											    VimContext->CommandKeys).BytesWritten;
		} break;
	}
}

PLORE_VIM_COMMAND(TabTextFilterClear) {
	Tab->FilterState->GlobalListingFilter = ClearStruct(text_filter);
}

PLORE_VIM_COMMAND(TabTextFilterHide) {
	TextFilterHelper(VimContext, Tab, Command, TextFilterState_Hide, true);
}

PLORE_VIM_COMMAND(TabTextFilterShow) {
	TextFilterHelper(VimContext, Tab, Command, TextFilterState_Show, true);
}


PLORE_VIM_COMMAND(DirectoryTextFilterClear) {
	ForMap(FileContext->FileInfo, file_info_lookup) {
		It->V.Filter = ClearStruct(text_filter);
	}
	// TODO(Evan): Walk the directory listing table!
}

PLORE_VIM_COMMAND(DirectoryTextFilterHide) {
	TextFilterHelper(VimContext, Tab, Command, TextFilterState_Hide, false);
}

PLORE_VIM_COMMAND(DirectoryTextFilterShow) {
	TextFilterHelper(VimContext, Tab, Command, TextFilterState_Show, false);
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
	plore_path *Selected = GetImpliedSelectionPath(State);
	if (!Selected) return;

	switch (Command.State) {
		case VimCommandState_Start: {
			SetVimCommandFromString(VimContext, Selected->FilePart);
			InsertBegin(VimContext, Command);
		} break;

		case VimCommandState_Cancel:
		case VimCommandState_Incomplete: {
		} break;

		case VimCommandState_Finish: {
			if (Command.Shell && *Command.Shell) {
				plore_path_buffer Buffer = {0};
				StringCopy(Selected->Absolute, Buffer, ArrayCount(Buffer));
				Platform->PathPop(Buffer, ArrayCount(Buffer), true);
				char *NewAbsolute = StringConcatenate(Buffer, ArrayCount(Buffer), Command.Shell);
				if (Platform->RenameFile(Selected->Absolute, NewAbsolute)) MapReset(&FileContext->Selected);
			}
		} break;

		InvalidDefaultCase;
	}

}

PLORE_VIM_COMMAND(Select) {
	plore_file_listing_info_get_or_create_result CursorResult = GetOrCreateFileInfo(FileContext, &Tab->DirectoryState->Current.File.Path);
	plore_file *Selectee = Tab->DirectoryState->Current.Entries + CursorResult.Info->Cursor;
	ToggleSelected(Tab->FileContext->Selected, &Tab->DirectoryState->Cursor.File.Path);
}

PLORE_VIM_COMMAND(NewTab) {
	if (Command.Pattern[0]) {
		i32 NewTab = StringToI32(Command.Pattern);
		if (NewTab <= 0)                           return;
		else if (NewTab > ArrayCount(State->Tabs)) return;

		NewTab -= 1;
		SetCurrentTab(State, NewTab);
	}
}

PLORE_VIM_COMMAND(CloseTab) {
	if (State->TabCount > 1) {
		u64 TabIndex = 0xdeadbeef;
		for (u64 T = 0; T < ArrayCount(State->Tabs); T++) {
			plore_tab *Other = State->Tabs + T;
			if (Tab == Other) {
				TabIndex = T;
				break;
			}
		}

		// NOTE(Evan): Find and switch to the closest tab, preferring numerically smaller tab indices.
		Assert(TabIndex < ArrayCount(State->Tabs));

		i64 tBefore = -1;
		i64 tAfter = -1;
		for (u64 T = 0; T < ArrayCount(State->Tabs); T++) {
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
			if (tAfter == -1) tTarget = tBefore;
			else              tTarget = tAfter;
		} else {
			if (tBefore == -1) tTarget = tAfter;
			else               tTarget = tBefore;
		}
		Assert(tTarget < ArrayCount(State->Tabs));
		Assert(State->Tabs[tTarget].Active);

		ClearTab(State, TabIndex);
		SetCurrentTab(State, tTarget);

	}
}

PLORE_VIM_COMMAND(OpenShell) {
	Platform->RunShell((platform_run_shell_desc) {
						   .Command = Command.Shell,
					   });
}

PLORE_VIM_COMMAND(CreateFile) {
	if (Command.Shell) {
		char *FilePath = PushBytes(&State->FrameArena, PLORE_MAX_PATH);
		Platform->PathJoin(FilePath, Tab->DirectoryState->Current.File.Path.Absolute, Command.Shell);

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
			plore_path *Selection = GetImpliedSelectionPath(State);
			if (Selection) {
				Platform->DeleteFile(Selection->Absolute, (platform_delete_file_desc) {
											  .Recursive = false,
											  .PermanentDelete = false,
										  });
			} else if (MapCount(FileContext->Selected)) {
				ForMap(FileContext->Selected, file_lookup) {
					Platform->DeleteFile(It->K, (platform_delete_file_desc) {
											 .Recursive = false,
											 .PermanentDelete = false,
										 });
				}

				MapReset(FileContext->Selected);
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
		ToggleYanked(State->Yanked, &File->Path);
	}
}

PLORE_VIM_COMMAND(SelectAll) {
	plore_file_listing *CurrentDirectory = &Tab->DirectoryState->Current;
	for (u64 F = 0; F < CurrentDirectory->Count; F++) {
		plore_file *File = CurrentDirectory->Entries + F;
		ToggleSelected(Tab->FileContext->Selected, &File->Path);
	}
}

PLORE_VIM_COMMAND(ShowHiddenFiles) {
	Tab->FilterState->HideMask.HiddenFiles = !Tab->FilterState->HideMask.HiddenFiles;
}

internal void
PloreSortHelper(plore_tab *Tab, file_sort_mask SortMask) {
	if (Tab->FilterState->SortMask != SortMask) Tab->FilterState->SortAscending = false;
	else                                        Tab->FilterState->SortAscending = !Tab->FilterState->SortAscending;

	Tab->FilterState->SortMask = SortMask;
}

PLORE_VIM_COMMAND(ClearSelect) {
	MapReset(&FileContext->Selected);
}

PLORE_VIM_COMMAND(ToggleSortName) {
	PloreSortHelper(Tab, FileSort_Name);
}

PLORE_VIM_COMMAND(ToggleSortSize) {
	PloreSortHelper(Tab, FileSort_Size);
}

PLORE_VIM_COMMAND(ToggleSortModified) {
	PloreSortHelper(Tab, FileSort_Modified);
}

PLORE_VIM_COMMAND(ToggleSortExtension) {
	PloreSortHelper(Tab, FileSort_Extension);
}

PLORE_VIM_COMMAND(ShowCommandList) {
	switch (Command.State) {
		case VimCommandState_Start: {
			// CUTNPASTE(Evan): From OpenFile;
			// Not worth making a utility function for 2-dimensional strcpy() swizzling, which should frankly be done at compile-time.

			// NOTE(Evan): Don't copy VimCommandType_None!
			char *TitleBuffer[VimListing_ListSize] = {0};
			for (u64 L = 0; L < VimCommandType_Count-1; L++) {
				TitleBuffer[L] = PushBytes(&State->FrameArena, VimListing_Size);
				StringCopy(VimCommandNames[L+1], TitleBuffer[L], VimListing_Size);
			}

			char *SecondaryBuffer[VimListing_ListSize] = {0};
			for (u64 L = 0; L < VimCommandType_Count-1; L++) {
				SecondaryBuffer[L] = PushBytes(&State->FrameArena, VimListing_Size);
				StringCopy(VimCommandDescriptions[L+1], SecondaryBuffer[L], VimListing_Size);
			}

			ListerBegin(VimContext, (vim_lister_begin_desc) {
							.Command = Command,
							.Titles = TitleBuffer,
							.Secondaries = SecondaryBuffer,
							.Count = VimCommandType_Count-1,
							.HideFiles = true,
						});
		} break;

		case VimCommandState_Finish: {
			// NOTE(Evan): Cursor is offset to account for VimCommandType_None.
			Command = (vim_command) {
				.State = VimCommandState_Start,
				.Type = VimContext->ListerState.Cursor+1,
			};

			ResetVimState(VimContext);
            PLORE_DO_VIM_COMMAND_ENUM(Command.Type);

		} break;
	}

}

PLORE_VIM_COMMAND(FontScaleIncrease) {
	if (State->Font->CurrentFont < PloreBakedFont_Count-1) State->Font->CurrentFont += 1;
}

PLORE_VIM_COMMAND(FontScaleDecrease) {
	if (State->Font->CurrentFont > 0) State->Font->CurrentFont -= 1;
}

PLORE_VIM_COMMAND(ToggleFileMetadata) {
	Tab->FilterState->MetadataDisplay = ToggleFlag(Tab->FilterState->MetadataDisplay, FileMetadataDisplay_Basic);
}

PLORE_VIM_COMMAND(ToggleExtendedFileMetadata) {
	Tab->FilterState->MetadataDisplay = ToggleFlag(Tab->FilterState->MetadataDisplay, FileMetadataDisplay_Extended);
}

PLORE_VIM_COMMAND(ExitPlore) {
	DrawText("ExitPlore");
	State->ShouldQuit = true;
}

#define PLORE_X(Name, Ignored1, _Ignored2, _Ignored3) Do##Name,
vim_command_function *VimCommands[] = {
	VIM_COMMANDS
};
#undef PLORE_X
