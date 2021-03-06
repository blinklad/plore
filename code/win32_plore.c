#include "plore.h"
#include "plore_time.h"
#include "plore_common.h"
#include "plore_memory.h"
#include "plore_platform.h"

#include "win32_plore.h"


global plore_input GlobalPloreInput;
global windows_timer GlobalWindowsTimer;      // TODO(Evan): Timing!
global windows_context *GlobalWindowsContext;
global b64 GlobalRunning = true;

PLATFORM_DEBUG_ASSERT_HANDLER(WindowsDebugAssertHandler) {
	b64 Result = false;
	int Action = MessageBox(GlobalWindowsContext->Window,
							Message,
							"Plore Assertion",
							MB_ICONEXCLAMATION | MB_CANCELTRYCONTINUE | MB_DEFBUTTON2);

	switch (Action) {
		case IDCANCEL: {
			_exit(-1);
		} break;

		case IDTRYAGAIN: {
			Result = true;
		} break;

		case IDCONTINUE: {
			Result = false;
		} break;

		InvalidDefaultCase;
	}

	return(Result);

}

#include "win32_gl_loader.c"

// NOTE(Evan): For GL bits!
PLATFORM_DEBUG_PRINT_LINE(WindowsPrintLine);
PLATFORM_DEBUG_PRINT(WindowsDebugPrint);
#define PrintLine WindowsPrintLine
#define Print WindowsDebugPrint

#include "plore_string.h"

#include "plore_memory.c"
#include "plore_gl.c"

internal plore_time
WindowsFiletimeToPloreTime(FILETIME Filetime) {
	plore_time Result = {0};
	ULARGE_INTEGER BigBoy = {0};

	BigBoy.LowPart  = Filetime.dwLowDateTime;
	BigBoy.HighPart = Filetime.dwHighDateTime;
	Result.T = BigBoy.QuadPart / 10000000ULL - 11644473600ULL;

	return(Result);
}

internal FILETIME
PloreTimeToWindowsFiletime(plore_time Time)
{
	FILETIME Result = {0};
    ULARGE_INTEGER BigBoy = {0};

    BigBoy.QuadPart = (Time.T * 10000000LL) + 116444736000000000LL;
    Result.dwLowDateTime = BigBoy.LowPart;
    Result.dwHighDateTime = BigBoy.HighPart;

	return(Result);
}

// NOTE(Evan): Platform layer implementation.
PLATFORM_DEBUG_PRINT(WindowsDebugPrint) {
    va_list Args;
    va_start(Args, Format);
    local char Buffer[256];
    stbsp_vsnprintf(Buffer, 256, Format, Args);
    fprintf(stdout, Buffer);
    va_end(Args);
    OutputDebugStringA(Buffer);
}

PLATFORM_DEBUG_PRINT_LINE(WindowsPrintLine) {
    va_list Args;
    va_start(Args, Format);
    local char Buffer[256];
    stbsp_vsnprintf(Buffer, 256, Format, Args);
    fprintf(stdout, Buffer);
    fprintf(stdout, "\n");
    va_end(Args);
}


PLATFORM_DEBUG_OPEN_FILE(WindowsDebugOpenFile) {
    HANDLE TheFile = CreateFileA(Path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD Error = GetLastError();
    DWORD FileSize = GetFileSize(TheFile, NULL);

    platform_readable_file Result = {
        .Opaque = TheFile,
        .OpenedSuccessfully = (TheFile != INVALID_HANDLE_VALUE),
        .FileSize = FileSize,
    };

    return(Result);

}

PLATFORM_DEBUG_CLOSE_FILE(WindowsDebugCloseFile) {
	CloseHandle((HANDLE) File.Opaque);
}

// NOTE(Evan): This does not work for files > 4gb!
PLATFORM_DEBUG_READ_ENTIRE_FILE(WindowsDebugReadEntireFile) {
    platform_read_file_result Result = {0};

    HANDLE FileHandle = File.Opaque;
    DWORD BytesRead;

    Assert(BufferSize < UINT32_MAX);
    if (File.FileSize <= BufferSize) {
	    DWORD BufferSize32 = (DWORD) BufferSize;

	    Result.ReadSuccessfully = ReadFile(FileHandle, Buffer, BufferSize32, &BytesRead, NULL);
	    Assert(BytesRead == File.FileSize);
	    Result.BytesRead = BytesRead;
	    Result.Buffer = Buffer;
	}

    return(Result);
}

PLATFORM_DEBUG_READ_ENTIRE_FILE(WindowsDebugReadFileSize) {
    platform_read_file_result Result = {0};

    HANDLE FileHandle = File.Opaque;
    DWORD BytesRead;

    Assert(BufferSize < UINT32_MAX);
    DWORD BufferSize32 = (DWORD) BufferSize;

    Result.ReadSuccessfully = ReadFile(FileHandle, Buffer, BufferSize32, &BytesRead, NULL);
    Result.BytesRead = BytesRead;
    Result.Buffer = Buffer;

    return(Result);
}

// CLEANUP(Evan): Renderer bits.
PLATFORM_CREATE_TEXTURE_HANDLE(WindowsGLCreateTextureHandle) {
	platform_texture_handle Result = {
		.Width = Desc.Width,
		.Height = Desc.Height,
	};

	glEnable(GL_TEXTURE_2D);

	GLenum GLProvidedPixelFormat = 0;
	GLenum GLTargetPixelFormat = 0;
	GLenum GLFilterMode = 0;
	switch (Desc.TargetPixelFormat) {
		case PixelFormat_RGBA8: {
			GLTargetPixelFormat = GL_RGBA8;
		} break;

		case PixelFormat_ALPHA: {
			GLTargetPixelFormat = GL_ALPHA;
		} break;

		case PixelFormat_BGRA8: {
			GLTargetPixelFormat = GL_BGRA;
		} break;

		InvalidDefaultCase;
	}
	switch (Desc.ProvidedPixelFormat) {

		case PixelFormat_RGBA8: {
			GLProvidedPixelFormat = GL_RGBA;
		} break;

		case PixelFormat_RGB8: {
			GLProvidedPixelFormat = GL_RGB;
		} break;

		case PixelFormat_ALPHA: {
			GLProvidedPixelFormat = GL_ALPHA;
		} break;

		case PixelFormat_BGRA8: {
			GLProvidedPixelFormat = GL_BGRA;
		} break;

		InvalidDefaultCase;
	}

	switch (Desc.FilterMode) {
		case FilterMode_Linear: {
			GLFilterMode = GL_LINEAR;
		} break;

		case FilterMode_Nearest: {
			GLFilterMode = GL_NEAREST;
		} break;

		InvalidDefaultCase;
	}

	GLuint Handle;
	glGenTextures(1, &Handle);
	Result.Opaque = Handle;
	glBindTexture(GL_TEXTURE_2D, Result.Opaque);

	glTexImage2D(GL_TEXTURE_2D, 0, GLTargetPixelFormat, Result.Width, Result.Height, 0, GLProvidedPixelFormat, GL_UNSIGNED_BYTE, Desc.Pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLFilterMode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLFilterMode);


	return(Result);
}

PLATFORM_DESTROY_TEXTURE_HANDLE(WindowsGLDestroyTextureHandle) {
	GLuint Opaque = (GLuint) Texture.Opaque;
	glDeleteTextures(1, &Opaque);
}

// NOTE(Evan): Windows decrements a signed counter whenever FALSE is provided, so there will be some weirdness to this.
PLATFORM_SHOW_CURSOR(WindowsShowCursor) {
	GlobalPloreInput.ThisFrame.CursorIsShowing = Show;
	DWORD CursorCount = ShowCursor(Show);
	WindowsPrintLine("CursorCount :: %d", CursorCount);
}


global WINDOWPLACEMENT GlobalWindowPlacement = { .length = sizeof(GlobalWindowPlacement) };

PLATFORM_TOGGLE_FULLSCREEN(WindowsToggleFullscreen) {
	HWND Window = GlobalWindowsContext->Window;
	DWORD WindowStyle = GetWindowLong(Window, GWL_STYLE);

	if (WindowStyle & WS_OVERLAPPEDWINDOW) {
		MONITORINFO MonitorInfo = { .cbSize = sizeof(MonitorInfo) };

		if(GetWindowPlacement(Window, &GlobalWindowPlacement)
		   && GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY),
							 &MonitorInfo)) {
			SetWindowLong(Window, GWL_STYLE, WindowStyle & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(Window,
						 HWND_TOP,
						 MonitorInfo.rcMonitor.left,
						 MonitorInfo.rcMonitor.top,
						 MonitorInfo.rcMonitor.right  - MonitorInfo.rcMonitor.left,
						 MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
						 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	} else {
		SetWindowLong(Window, GWL_STYLE, WindowStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(Window, &GlobalWindowPlacement);
		SetWindowPos(Window,
					 NULL,
					 0,
					 0,
					 0,
					 0,
					 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER
					 | SWP_FRAMECHANGED);
	}
}

PLATFORM_GET_CURRENT_DIRECTORY_PATH(WindowsGetCurrentDirectoryPath) {
	plore_path Result = {0};

	GetCurrentDirectory(ArrayCount(Result.Absolute), Result.Absolute);
	char *FilePart = PathFindFileName(Result.Absolute);
	StringCopy(FilePart, Result.FilePart, ArrayCount(Result.FilePart));

	return(Result);
}
PLATFORM_SET_CURRENT_DIRECTORY(WindowsSetCurrentDirectory) {
	b64 Result = SetCurrentDirectory(Path);

	return(Result);
}

PLATFORM_PATH_POP(WindowsPathPop) {
	platform_path_pop_result Result = {
		.DidRemoveSomething = true,
		.AbsolutePath = Buffer,
	};
	PathRemoveBackslash(Buffer);
	Result.DidRemoveSomething = PathRemoveFileSpecA(Buffer) != 0;
	if (AddTrailingSlash && Result.DidRemoveSomething) {
		u64 Length = StringLength(Buffer);
		Assert(Length < PLORE_MAX_PATH);
		Buffer[Length++] = '\\';
		Buffer[Length++] = '\0';
	}
	Result.FilePart = PathFindFileName(Result.AbsolutePath);

	return(Result);
}

PLATFORM_PATH_IS_DIRECTORY(WindowsPathIsDirectory) {
	b64 Result = PathIsDirectory(Buffer);
	return(Result);
}

PLATFORM_PATH_IS_TOP_LEVEL(WindowsPathIsTopLevel) {
	return(PathIsRoot(Buffer));
}


PLATFORM_PATH_JOIN(WindowsPathJoin) {
	b32 Result = true;
	b64 WeAreTopLevel = WindowsPathIsTopLevel(First);

	u64 BytesWritten = StringCopy(First, Buffer, PLORE_MAX_PATH);
	if (!WeAreTopLevel) Buffer[BytesWritten++] = '\\';

	BytesWritten += StringCopy(Second, Buffer + BytesWritten, PLORE_MAX_PATH-BytesWritten);
	return(Result);
}


// NOTE(Evan): Returns whether the path
internal b64
WindowsMakePathSearchable(char *Directory, char *Buffer, u64 Size) {
	u64 BytesWritten = StringCopy(Directory, Buffer, Size);
	u64 SearchBytesWritten = 0;
	char SearchChars[] = {'\\', '*'};
	b64 SearchCanFit = BytesWritten < (Size + sizeof(SearchChars));
	if (!SearchCanFit) return(false);

	b64 IsDirectoryRoot = PathIsRoot(Buffer);
	if (!IsDirectoryRoot) Buffer[BytesWritten + SearchBytesWritten++] = '\\';

	Buffer[BytesWritten + SearchBytesWritten++] = '*';
	Buffer[BytesWritten + SearchBytesWritten++] = '\0';

	return(true);
}

enum {
	Taskmaster_MaxThreads = 4,
	Taskmaster_MaxTasks = 32,
	Taskmaster_TaskArenaSize = Megabytes(1),
};
typedef struct platform_taskmaster {
	platform_task Tasks[Taskmaster_MaxTasks];
	LONG64        volatile TaskCount;
	LONG64        volatile TaskCursor;
} platform_taskmaster;

#define FullMemoryBarrier MemoryBarrier(); _ReadWriteBarrier();
HANDLE TaskAvailableSemaphore;
HANDLE SearchSemaphore;

DWORD WINAPI
WindowsThreadStart(void *Param);

typedef struct plore_thread_context {
	u64 LogicalID;
	void *Opaque;
	platform_taskmaster *Taskmaster;
	char Pad[40]; // NOTE(Evan): Pad out to 64 bytes.
} plore_thread_context;
internal void
InitTaskmaster(memory_arena *Arena, platform_taskmaster *Taskmaster) {
	for (u64 T = 0; T < ArrayCount(Taskmaster->Tasks); T++) {
		Taskmaster->Tasks[T] = (platform_task) {
			.Arena = SubArena(Arena, Taskmaster_TaskArenaSize, 16),
		};
	}


	TaskAvailableSemaphore = CreateSemaphore(NULL,
										     0,
										     Taskmaster_MaxTasks,
										     NULL);

	Assert(TaskAvailableSemaphore);

	SearchSemaphore = CreateSemaphore(NULL,
								      0,
								      1,
								      NULL);

	Assert(SearchSemaphore);

	plore_thread_context Threads[Taskmaster_MaxThreads] = {0};
	for (u64 T = 0; T < ArrayCount(Threads); T++) {
		// NOTE(Evan): Create thread in suspended state then immediately resume it, so we can guarantee its context has a valid handle before it kicks off.
		Threads[T] = (plore_thread_context) {
			.LogicalID = T,
			.Opaque = CreateThread(0,
								   0,
								   WindowsThreadStart,
								   Threads+T,
								   CREATE_SUSPENDED,
								   0),
			.Taskmaster = Taskmaster,
		};

		ResumeThread(Threads[T].Opaque);
	}

}

typedef struct platform_get_next_task_result {
	platform_task *Task;
	u64 Index;
} platform_get_next_task_result;
internal platform_get_next_task_result
_GetNextTask(platform_taskmaster *Taskmaster) {
	platform_get_next_task_result Result = {0};

	Result.Index = InterlockedIncrement(&(LONG)Taskmaster->TaskCount)-1;
	u64 CircularIndex = Result.Index % ArrayCount(Taskmaster->Tasks);
	b64 Running = Taskmaster->Tasks[CircularIndex].Running;

	// TODO(Evan): Upper bound for tasks and error handling for wraparound on full. We shouldn't have many after all.
	Assert(!Running);
	FullMemoryBarrier;

	Result.Task = Taskmaster->Tasks + CircularIndex;
	memory_arena Old = Result.Task->Arena;
	*Result.Task = ClearStruct(platform_task);
	Result.Task->Arena = Old;
	ClearArena(&Result.Task->Arena);

	return(Result);
}


PLATFORM_CREATE_TASK_WITH_MEMORY(WindowsCreateTaskWithMemory) {
	platform_get_next_task_result NextTaskResult = _GetNextTask(Taskmaster);
	task_with_memory_handle Result = {
		.Arena = &NextTaskResult.Task->Arena,
		.ID = NextTaskResult.Index,
	};

	return(Result);
}

PLATFORM_START_TASK_WITH_MEMORY(WindowsStartTaskWithMemory) {
	platform_task *NextTask = Taskmaster->Tasks + (Handle.ID % ArrayCount(Taskmaster->Tasks));
	NextTask->Procedure = Procedure;
	NextTask->Param = Param;
	FullMemoryBarrier;

	ReleaseSemaphore(TaskAvailableSemaphore, 1, NULL);
}

PLATFORM_PUSH_TASK(WindowsPushTask) {
	platform_task *NextTask = _GetNextTask(Taskmaster).Task;
	NextTask->Procedure = Procedure;
	NextTask->Param = Param;
	FullMemoryBarrier;

	ReleaseSemaphore(TaskAvailableSemaphore, 1, NULL);
}

// NOTE(Evan): This size is empirically validated to be pretty reasonable, although not 100% accurate.
#define MAX_SEARCH_DIRECTORIES 2048
char ChildStack[MAX_SEARCH_DIRECTORIES][PLORE_MAX_PATH];
u64 ChildStackCount;
volatile LONG64 SearchActive;

PLATFORM_TASK(WindowsGetDirectorySize) {
	plore_directory_query_state *State = (plore_directory_query_state *)Param;

	StringCopy(State->Path.Absolute, ChildStack[ChildStackCount++], ArrayCount(ChildStack[0]));

	u64 SizeAccum = 0;
	u64 FileAccum = 0;
	u64 DirectoryAccum = 0;

	while (ChildStackCount) {
		if (!SearchActive) {
			break;
		}

		WIN32_FIND_DATA FindData = {0};

		char ThisDirectory[PLORE_MAX_PATH] = {0};
		StringCopy(ChildStack[--ChildStackCount], ThisDirectory, ArrayCount(ThisDirectory));

		char SearchableDirectory[PLORE_MAX_PATH] = {0};
		if (!WindowsMakePathSearchable(ThisDirectory, SearchableDirectory, ArrayCount(SearchableDirectory))) {
			break;
		}

		HANDLE FindHandle = FindFirstFile(SearchableDirectory, &FindData);
		u64 IgnoredCount = 0;
		if (FindHandle != INVALID_HANDLE_VALUE) {
			do {
				if (FindData.cFileName[0] == '.' || FindData.cFileName[0] == '$') continue;

				DWORD FlagsToIgnore = FILE_ATTRIBUTE_DEVICE     |
					FILE_ATTRIBUTE_ENCRYPTED |
					FILE_ATTRIBUTE_OFFLINE   |
					FILE_ATTRIBUTE_TEMPORARY;

				if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					DirectoryAccum += 1;
					if (ChildStackCount < MAX_SEARCH_DIRECTORIES) {
						char *ChildStackEntry = ChildStack[ChildStackCount++];

						// NOTE(Evan): Make the file name an absolute path.
						u64 BytesWritten = StringCopy(ThisDirectory, ChildStackEntry, PLORE_MAX_PATH);
						ChildStackEntry[BytesWritten++] = '\\';
						StringCopy(FindData.cFileName, ChildStackEntry+BytesWritten, PLORE_MAX_PATH-BytesWritten);
					}
				} else if (!(FindData.dwFileAttributes & FlagsToIgnore)) {
					FileAccum += 1;
					SizeAccum += (FindData.nFileSizeHigh * (MAXDWORD+1)) + FindData.nFileSizeLow;
				}
			} while (FindNextFile(FindHandle, &FindData));

			FindClose(FindHandle);
		}

		State->SizeProgress = SizeAccum;
		State->FileCount = FileAccum;
		State->DirectoryCount = DirectoryAccum;
	}

	ChildStackCount = 0;
	State->Completed = true;

	// NOTE(Evan): If the search was cancelled by a new task, signal that we're done.
	// Otherwise, toggle the search as usual.
	if (!SearchActive) {
		ReleaseSemaphore(SearchSemaphore, 1, NULL);
	} else {
		InterlockedExchange64(&SearchActive, false);
	}
}



PLATFORM_DIRECTORY_SIZE_TASK_BEGIN(WindowsDirectorySizeTaskBegin) {
	// NOTE(Evan): Check if there is a search active, and if we interrupted it, wait until it signals completion
	// before pushing a new search task.
	if (SearchActive) {
		b64 SignalledStopSearch = InterlockedCompareExchange64(&SearchActive, false, true);
		if (SignalledStopSearch) {
			WaitForSingleObject(SearchSemaphore, INFINITE);
		} // Otherwise, it happened to finish before we could toggle the flag.
	}

	*State = (plore_directory_query_state) {
		.Path = *Path,
	};
	InterlockedExchange64(&SearchActive, true);
	FullMemoryBarrier;

	WindowsPushTask(Taskmaster, WindowsGetDirectorySize, State);
}

// NOTE(Evan): Directory name should not include trailing '\' nor any '*' or '?' wildcards.
PLATFORM_GET_DIRECTORY_ENTRIES(WindowsGetDirectoryEntries) {
	directory_entry_result Result = {
		.Name = DirectoryName,
		.Buffer = Buffer,
		.Size = Size,
	};

	char SearchableDirectoryName[PLORE_MAX_PATH] = {0};
	if (!WindowsMakePathSearchable(DirectoryName, SearchableDirectoryName, ArrayCount(SearchableDirectoryName))) {
		return(Result);
	}

	b64 IsDirectoryRoot = PathIsRoot(DirectoryName);

	WIN32_FIND_DATA FindData = {0};
	HANDLE FindHandle = FindFirstFile(SearchableDirectoryName, &FindData);
	u64 IgnoredCount = 0;
	if (FindHandle != INVALID_HANDLE_VALUE) {
		do {
			if (Result.Count >= Result.Size) {
				WindowsPrintLine("Directory entry capacity reached when querying %s", SearchableDirectoryName);
				break; // @Hack
			}

			char *FileName = FindData.cFileName;
			if (FileName[0] == '.' || FileName[0] == '$') continue;

			plore_file *File = Buffer + Result.Count;

			DWORD FlagsToIgnore = FILE_ATTRIBUTE_DEVICE    |
								  FILE_ATTRIBUTE_ENCRYPTED |
								  FILE_ATTRIBUTE_OFFLINE   |
								  FILE_ATTRIBUTE_TEMPORARY;

			if (FindData.dwFileAttributes & FlagsToIgnore) {
				Result.IgnoredCount++;
				continue;
			}

			if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				File->Type = PloreFileNode_Directory;
			} else { // NOTE(Evan): Assume a 'normal' file.
				File->Type = PloreFileNode_File;
				File->Extension = GetFileExtension(FindData.cFileName).Extension;
			}

			File->Bytes = (FindData.nFileSizeHigh * (MAXDWORD+1)) + FindData.nFileSizeLow;
			File->LastModification = WindowsFiletimeToPloreTime(FindData.ftLastWriteTime);
			File->Hidden = FindData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN;


			u64 BytesWritten = StringCopy(DirectoryName, File->Path.Absolute, ArrayCount(File->Path.Absolute));
			Assert(BytesWritten < ArrayCount(File->Path.Absolute) - 2); // NOTE(Evan): For trailing '\';

			if (!IsDirectoryRoot) File->Path.Absolute[BytesWritten++] = '\\';

			// @Cleanup, use PathFindFileName();
			StringCopy(FindData.cFileName, File->Path.Absolute + BytesWritten, ArrayCount(File->Path.Absolute) - BytesWritten);
			StringCopy(FindData.cFileName, File->Path.FilePart, ArrayCount(File->Path.Absolute));


			Result.Count++;

		} while (FindNextFile(FindHandle, &FindData));

		FindClose(FindHandle);
	} else {
		WindowsPrintLine("Could not open directory %s", SearchableDirectoryName);
	}

	Result.Succeeded = true;
	return(Result);
}

PLATFORM_CREATE_FILE(WindowsCreateFile) {
	b64 Result = false;
	DWORD OverwriteFlags = CREATE_NEW;
	if (OverwriteExisting) OverwriteFlags = CREATE_ALWAYS;

	HANDLE TheFile = CreateFile(Path,
								GENERIC_READ | GENERIC_WRITE,
								FILE_SHARE_READ | FILE_SHARE_WRITE,
								0,
								OverwriteFlags,
								FILE_ATTRIBUTE_NORMAL,
								0);

	Result = TheFile != INVALID_HANDLE_VALUE;
	if (Result) {
		Result = CloseHandle(TheFile) != 0;
	}

	return(Result);
}

PLATFORM_CREATE_DIRECTORY(WindowsCreateDirectory) {
	b64 Result = CreateDirectory(Path, 0);
	return(Result);
}

// TODO(Evan): Remove wildcards!
PLATFORM_DELETE_FILE(WindowsDeleteFile) {
	b64 Result = false;

	char PathDoubleNulled[PLORE_MAX_PATH+1] = {0};
	u64 BytesWritten = StringCopy(Path, PathDoubleNulled, ArrayCount(PathDoubleNulled));
	Assert(BytesWritten <= PLORE_MAX_PATH);
	PathDoubleNulled[BytesWritten+1] = '\0';

	FILEOP_FLAGS Flags = FOF_SILENT;
	if (!Desc.PermanentDelete) Flags |= FOF_ALLOWUNDO;
	if (!Desc.Recursive)       Flags |= FOF_NORECURSION; // NOTE(Evan): Recursive by default.

	SHFILEOPSTRUCT ShellOptions = {
		.hwnd = GlobalWindowsContext->Window,
		.wFunc = FO_DELETE,
		.pFrom = PathDoubleNulled,
		.fFlags = Flags,
	};

	Result = SHFileOperation(&ShellOptions) == 0;

	return(Result);
}

PLATFORM_MOVE_FILE(WindowsMoveFile) {
	b64 Result = false;
	Assert(dAbsolutePath);

	char sPathDoubleNulled[PLORE_MAX_PATH+1] = {0};
	{
		u64 BytesWritten = StringCopy(sAbsolutePath, sPathDoubleNulled, ArrayCount(sPathDoubleNulled));
		Assert(BytesWritten <= PLORE_MAX_PATH);
		sPathDoubleNulled[BytesWritten+1] = '\0';
	}
	char dPathDoubleNulled[PLORE_MAX_PATH+1] = {0};
	{
		u64 BytesWritten = StringCopy(dAbsolutePath, dPathDoubleNulled, ArrayCount(dPathDoubleNulled));
		Assert(BytesWritten <= PLORE_MAX_PATH);
		dPathDoubleNulled[BytesWritten+1] = '\0';
	}

	FILEOP_FLAGS Flags = FOF_SIMPLEPROGRESS;//FOF_SILENT;
	SHFILEOPSTRUCT ShellOptions = {
		.hwnd = GlobalWindowsContext->Window,
		.wFunc = FO_MOVE,
		.pFrom = sPathDoubleNulled,
		.pTo = dPathDoubleNulled,
		.fFlags = Flags,
	};

	Result = SHFileOperation(&ShellOptions) == 0;
	return(Result);
}

PLATFORM_RENAME_FILE(WindowsRenameFile) {
	b64 Result = WindowsMoveFile(sAbsolutePath, dAbsolutePath);
	return(Result);
}

internal void CALLBACK
CleanupProcessHandle(void *Context, BOOLEAN WasTimedOut);

global CRITICAL_SECTION ProcessHandleTableGuard;
typedef struct process_handle {
	PROCESS_INFORMATION ProcessInfo;
	HANDLE WaitHandle;
	b64 NeedsCallbackCleanup;
	b64 IsRunning;
} process_handle;

global process_handle ProcessHandles[4];
global u64 ProcessHandleCursor;

PLATFORM_RUN_SHELL(WindowsRunShell) {
	b64 Result = false;

	local b64 Initialised = false;
	if (!Initialised) {
		Initialised = true;
		InitializeCriticalSection(&ProcessHandleTableGuard);
	}

	// @Cleanup
	char Buffer[PLORE_MAX_PATH] = {0};
	if (Desc.QuoteArgs) {
		StringPrintSized(Buffer, PLORE_MAX_PATH, "cmd.exe /s /k \"\"%s\" \"%s\"\"", Desc.Command, Desc.Args);
	} else if (Desc.Args) {
		StringPrintSized(Buffer, PLORE_MAX_PATH, "cmd.exe /s /k \"%s %s\"", Desc.Command, Desc.Args);
	} else {
		StringPrintSized(Buffer, PLORE_MAX_PATH, "cmd.exe /s /k \"%s\"", Desc.Command);
	}

	WindowsPrintLine("%s", Buffer);

	EnterCriticalSection(&ProcessHandleTableGuard);

	process_handle *MyProcess = ProcessHandles + ProcessHandleCursor;
	ProcessHandleCursor = (ProcessHandleCursor + 1) % ArrayCount(ProcessHandles);

	// NOTE(Evan): If we have wrapped around and this process is not finished, we should clean it up first.
	if (MyProcess->IsRunning) {
		WindowsPrintLine("Cleaned up process prematurely.");

		CloseHandle(MyProcess->ProcessInfo.hThread);

		DWORD MaybeExitCode = 0;
		if (GetExitCodeProcess(MyProcess->ProcessInfo.hProcess, &MaybeExitCode)) {
			if (MaybeExitCode == STILL_ACTIVE) {
				TerminateProcess(MyProcess->ProcessInfo.hProcess, 1);
			}
		}
		CloseHandle(MyProcess->ProcessInfo.hProcess);

		MyProcess->NeedsCallbackCleanup = true;
		MyProcess->IsRunning = false;
	}

	// NOTE(Evan): We cleanup the callback here because we aren't allowed to call UnregisterWaitEx() inside the
	// callback itself.
	// NOTE(Evan): Param indicates we want to wait for pending callbacks.
	if (MyProcess->NeedsCallbackCleanup) {
		UnregisterWaitEx(MyProcess->WaitHandle, INVALID_HANDLE_VALUE);
	}


	Result = CreateProcessA(NULL,
							Buffer,
							NULL,
							NULL,
							false,
							DETACHED_PROCESS,
							NULL,
							NULL,
							&(STARTUPINFOA) { .cb = sizeof(STARTUPINFOA) },
							&MyProcess->ProcessInfo);

	if (!Result) {
		DWORD Error = GetLastError();
		int BreakHere = 5;
	}

	b64 DidRegisterCleanup = RegisterWaitForSingleObject(&MyProcess->WaitHandle,
														 MyProcess->ProcessInfo.hProcess,
														 CleanupProcessHandle,
														 MyProcess,
														 INFINITE,
														 WT_EXECUTEONLYONCE);

	if (!DidRegisterCleanup) {
		WindowsPrintLine("Couldn't register %s for cleanup.", Desc.Args);
	}

	MyProcess->IsRunning = true;
	WindowsPrintLine("%s running command %s", Result ? "succeeded " : "failed ", Buffer);

	LeaveCriticalSection(&ProcessHandleTableGuard);

	return(Result);
}


internal void CALLBACK
CleanupProcessHandle(void *Context, BOOLEAN WasTimedOut) {
	EnterCriticalSection(&ProcessHandleTableGuard);

	process_handle *MyProcess = Context;
	if (MyProcess->IsRunning) {
		CloseHandle(MyProcess->ProcessInfo.hThread);

		DWORD MaybeExitCode = 0;
		if (GetExitCodeProcess(MyProcess->ProcessInfo.hProcess, &MaybeExitCode)) {
			if (MaybeExitCode == STILL_ACTIVE) {
				TerminateProcess(MyProcess->ProcessInfo.hProcess, 1);
			}
		}
		CloseHandle(MyProcess->ProcessInfo.hProcess);
		WindowsPrintLine("Cleaned up process successfully.");

		MyProcess->NeedsCallbackCleanup = true;
		MyProcess->IsRunning = false;
	}

	LeaveCriticalSection(&ProcessHandleTableGuard);
}



//
// NOTE(Evan): Internal API definitions.
//
OPENGL_DEBUG_CALLBACK(WindowsGLDebugMessageCallback)
{
    if (true)//severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM)
    {
        WindowsPrintLine("OpenGL error!... Not sure what, just an error!");
    }
}



//
// NOTE(Evan): This indicates that the PlatformAPI will need to expose platform-specific functions.
//
typedef struct windows_get_drives_result {
	char Drives[32];
	u64 CurrentDriveIndex;
} windows_get_drives_result;

internal windows_get_drives_result
WindowsGetDrives() {
	windows_get_drives_result Result = {0};
	DWORD DriveMask = GetLogicalDrives();
	for (u64 Bit = 1; Bit <= 32; Bit++) {
		if (DriveMask & (1 << Bit)) Result.Drives[Bit-1] = 'A' + Bit;
	}

	char Buffer[PLORE_MAX_PATH] = {0};
	GetCurrentDirectory(ArrayCount(Buffer), Buffer);
	char CurrentDriveLetter = Buffer[0];
	Assert((CurrentDriveLetter >= 'A' && CurrentDriveLetter <= 'Z') ||
		   (CurrentDriveLetter >= 'a' && CurrentDriveLetter <= 'z'));

	if (CurrentDriveLetter >= 'a' && CurrentDriveLetter <= 'z') {
		CurrentDriveLetter -= 32;
	}

	Result.CurrentDriveIndex = CurrentDriveLetter - 'A';

	return(Result);
}


internal void
DebugConsoleSetup() {
    // Log setup
	int ConsoleHandle = 0;
	intptr_t StreamHandle = 0;
	CONSOLE_SCREEN_BUFFER_INFO ConsoleScreenInfo = {0};
	FILE *Stream = 0;

	AllocConsole();

	// Set the screen buffer to be big enough to let us scroll text
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ConsoleScreenInfo);
	ConsoleScreenInfo.dwSize.Y = 500;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), ConsoleScreenInfo.dwSize);

	// Redirect unbuffered STDOUT, STDIN, STDERR to the console
    {
        StreamHandle = (intptr_t)GetStdHandle(STD_OUTPUT_HANDLE);
        ConsoleHandle = _open_osfhandle(StreamHandle, _O_TEXT);
        Stream = _fdopen(ConsoleHandle, "w");
        *stdout = *Stream;
        setvbuf(stdout, NULL, _IONBF, 0);
    }

    {
        StreamHandle = (intptr_t)GetStdHandle(STD_INPUT_HANDLE);
        ConsoleHandle = _open_osfhandle(StreamHandle, _O_TEXT);
        Stream = _fdopen(ConsoleHandle, "r");
        *stdin = *Stream;
        setvbuf(stdin, NULL, _IONBF, 0);
    }

    {
        StreamHandle = (intptr_t)GetStdHandle(STD_ERROR_HANDLE);
        ConsoleHandle = _open_osfhandle(StreamHandle, _O_TEXT);
        Stream = _fdopen(ConsoleHandle, "w");
        *stderr = *Stream;
        setvbuf(stderr, NULL, _IONBF, 0);
    }


	// Reopen streams required for console output by stdio
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
}

plore_inline windows_timer
WindowsGetTime() {
	local u64 Frequency = 0;
	if (!Frequency) {
		LARGE_INTEGER _Frequency;
		Assert(QueryPerformanceFrequency(&_Frequency));
		Frequency = _Frequency.QuadPart;
	}

	windows_timer CurrentTime = {0};
	CurrentTime.Frequency = Frequency;
	LARGE_INTEGER CurrentTicks;
	Assert(QueryPerformanceCounter(&CurrentTicks));
	CurrentTime.TicksNow = CurrentTicks.QuadPart;

	return(CurrentTime);
}

// more complex 4.x context:
// https://gist.github.com/mmozeiko/ed2ad27f75edf9c26053ce332a1f6647
internal windows_context
WindowsCreateAndShowOpenGLWindow(HINSTANCE Instance) {
    windows_context WindowsContext = {
        .Width = DEFAULT_WINDOW_WIDTH,
        .Height = DEFAULT_WINDOW_HEIGHT,
    };

    // Create a dummy window, pixel format, device context and rendering context,
    // just so we can get function pointers to wgl_ARB stuff!
    {
        WNDCLASSA dummy_winclass = {
            .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
            .lpfnWndProc = DefWindowProcA,
            .hInstance = GetModuleHandle(0),
            .lpszClassName = "Dummy Winclass",
        };

        RegisterClassA(&dummy_winclass);
        WindowsContext.Window = CreateWindowExA(
												0,
												"Dummy Winclass",  // Arbitrary WinClass descriptor
												"Dummy Winclass",
												0,
												CW_USEDEFAULT,
												CW_USEDEFAULT,
												CW_USEDEFAULT,
												CW_USEDEFAULT,
												NULL,
												NULL,
												Instance,
												NULL);


        PIXELFORMATDESCRIPTOR DesiredPixelFormat = {
            .nSize = sizeof(DesiredPixelFormat),
            .nVersion = 1,
            .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            .iPixelType = PFD_TYPE_RGBA,
            .cColorBits = 32,
            .cAlphaBits = 8,
            .cDepthBits = 24,
            .iLayerType = PFD_MAIN_PLANE,
        };

        WindowsContext.DeviceContext = GetDC(WindowsContext.Window);

        int32 PixelFormatID = ChoosePixelFormat(WindowsContext.DeviceContext, &DesiredPixelFormat);
        PIXELFORMATDESCRIPTOR SuggestedPixelFormat = {0};
        DescribePixelFormat(WindowsContext.DeviceContext, PixelFormatID, sizeof(PIXELFORMATDESCRIPTOR), &SuggestedPixelFormat);
        SetPixelFormat(WindowsContext.DeviceContext, PixelFormatID, &SuggestedPixelFormat);
        WindowsContext.OpenGLContext = wglCreateContext(WindowsContext.DeviceContext);

        if (!wglMakeCurrent(WindowsContext.DeviceContext, WindowsContext.OpenGLContext)) {
            WindowsPrintLine("Could not make context current!!!");
        }

        HMODULE opengl32_dll = LoadLibraryA("opengl32.dll");

		#define PLORE_X(Name, Return, Args) \
		Name = (PFN_ ## Name) wglGetProcAddress(#Name);\
		if (!Name) { \
		Name = (PFN_ ## Name) GetProcAddress(opengl32_dll, #Name);\
		}\

		        PLORE_GL_FUNCS
		#undef PLORE_X

        wglMakeCurrent(WindowsContext.DeviceContext, 0);
        wglDeleteContext(WindowsContext.OpenGLContext);
        ReleaseDC(WindowsContext.Window, WindowsContext.DeviceContext);
        DestroyWindow(WindowsContext.Window);
    }

    // Create our _actual_ window, a real (ARB) pixel format, device context, and rendering context.
    // The window is made current here.
    {
        WNDCLASSEX WinClass = {
            .cbSize = sizeof(WNDCLASSEX),
            .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
            .lpfnWndProc   = WindowsMessagePumpCallback,
            .hInstance     = Instance,
            .lpszClassName = "Plore Winclass",
            .hCursor = LoadCursor(NULL, IDC_ARROW),
        };

        RegisterClassEx(&WinClass);
        WindowsContext.Window = CreateWindowEx(
											   WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
											   "Plore Winclass",  // Arbitrary WinClass descriptor
											   "Plore",
											   WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SIZEBOX | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
											   0,       // posx
											   0,       // posy
											   DEFAULT_WINDOW_WIDTH,    // width
											   DEFAULT_WINDOW_WIDTH,    // height
											   NULL,
											   NULL,
											   Instance,
											   NULL);


        int DesiredPixelFormatAttributesARB[] = {
            WGL_DRAW_TO_WINDOW_ARB,     GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB,     GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB,      GL_TRUE,
            WGL_ACCELERATION_ARB,       WGL_FULL_ACCELERATION_ARB,
            WGL_PIXEL_TYPE_ARB,         WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,         32,
            WGL_DEPTH_BITS_ARB,         24,
            WGL_STENCIL_BITS_ARB,       8,
            0
        };
        WindowsContext.DeviceContext = GetDC(WindowsContext.Window);

        UINT CountOfMatchingFormatsWindowsCanFind = 0;
        int32 PixelFormatIDARB;
        if (!wglChoosePixelFormatARB(WindowsContext.DeviceContext,
									 DesiredPixelFormatAttributesARB,
									 0,
									 1,
									 &PixelFormatIDARB,
									 &CountOfMatchingFormatsWindowsCanFind) || CountOfMatchingFormatsWindowsCanFind == 0) {
            WindowsPrintLine("oh no");
        };

        PIXELFORMATDESCRIPTOR SuggestedPixelFormatARB = {0};
        DescribePixelFormat(WindowsContext.DeviceContext, PixelFormatIDARB, sizeof(PIXELFORMATDESCRIPTOR), &SuggestedPixelFormatARB);
        if (!SetPixelFormat(WindowsContext.DeviceContext, PixelFormatIDARB, &SuggestedPixelFormatARB)) {
            WindowsPrintLine("Could not set pixel format!!");
        }

        int OpenGLAttributes[] =  {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#if defined(PLORE_INTERNAL)
            // ask for debug context for non "Release" builds
            // this is so we can enable debug callback
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
            0,
        };

        WindowsContext.OpenGLContext = wglCreateContextAttribsARB(WindowsContext.DeviceContext, 0, OpenGLAttributes);

        if (!wglMakeCurrent(WindowsContext.DeviceContext, WindowsContext.OpenGLContext)) {
        }

#if PLORE_INTERNAL
		// enable debug callback
		glDebugMessageCallback(&WindowsGLDebugMessageCallback, NULL);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
    }

    // Make sure the window is visible and has the correct size!
    ShowWindow(WindowsContext.Window, SW_NORMAL);
    SetWindowPos(
				 WindowsContext.Window,
				 0,
				 0,
				 0,
				 WindowsContext.Width,
				 WindowsContext.Height,
				 0
				 );

	// NOTE(Evan): "Window Size" throughout the codebase really means "client rectangle", as in, the drawable area.
	DWORD Style = GetWindowLongPtr(WindowsContext.Window, GWL_STYLE );
	DWORD ExStyle = GetWindowLongPtr(WindowsContext.Window, GWL_EXSTYLE);
	HMENU Menu = GetMenu(WindowsContext.Window);

	RECT Rect = { 0, 0, WindowsContext.Width, WindowsContext.Height };

	AdjustWindowRectEx(&Rect, Style, Menu ? TRUE : FALSE, ExStyle);

	SetWindowPos(WindowsContext.Window, NULL, 0, 0, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_NOZORDER | SWP_NOMOVE );

	ShowCursor(TRUE);

    // 1/0 = Enable/disable vsync, -1 = adaptive sync.
    wglSwapIntervalEXT(1);

    return WindowsContext;
}

internal void
WindowsPushTextInput(keyboard_and_mouse *ThisFrame, MSG Message) {
	char C = Message.wParam;

	CheckedPush(ThisFrame->TextInput, ThisFrame->TextInputCount, C);
	StaticAssert(PloreKey_A == 1, "Assuming PloreKey_A-Z|0-9 follow contiguously.");
	plore_key Key = 0;
	if (C >= 'A' && C <= 'Z') {
		Key = PloreKey_None + Message.wParam - 'A' + 1;
	} else if (C >= '0' && C <= '9') {
		Key = PloreKey_Z    + Message.wParam - '0' + 1;
	} else {
		#define PLORE_X(Name, _Ignored, Character)     \
		case Character: {                              \
			Key = PloreKey_##Name;                     \
		} break;

		switch (C) {
			PLORE_KEYBOARD_AND_MOUSE

			case VK_ESCAPE: {
				Key = PloreKey_Escape;
				ThisFrame->TextInputCount--; // @HACK(Evan): Escape has no representation, but Windows "translates" it still. UGH.
			} break;
		}

		#undef PLORE_X
	}
	Assert(Key < PloreKey_Count);

	if (Key) {
		BYTE VKeys[256] = {0};
		GetKeyboardState(VKeys);

		b64 IsPressed = !(Message.lParam & (1 << 30)); // NOTE(Evan): 0 is pressed, 1 is released
		b64 IsDown = (Message.lParam & (1 << 29));
		ThisFrame->dKeys[Key] = IsDown;
		ThisFrame->pKeys[Key] = IsPressed;
		ThisFrame->sKeys[Key] = ThisFrame->pKeys[Key] && GetAsyncKeyState(VK_SHIFT); // @Cleanup
		ThisFrame->cKeys[Key] = ThisFrame->dKeys[PloreKey_Ctrl];
	}
}

internal void
WindowsProcessMessages(windows_context *Context, keyboard_and_mouse *ThisFrame) {
	MSG Message = {0};
	// NOTE(Evan): This is awful, but numeric input + ctrl is "translated" into garbage.
	ThisFrame->dKeys[PloreKey_Ctrl] = GetAsyncKeyState(VK_CONTROL);
	ThisFrame->pKeys[PloreKey_Ctrl] = ThisFrame->dKeys[PloreKey_Ctrl] && !GlobalPloreInput.LastFrame.dKeys[PloreKey_Ctrl];

	while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
	    switch (Message.message) {
	        case WM_KEYDOWN:
	        case WM_KEYUP:
	        {
	            b32 IsDown = Message.message == WM_KEYDOWN;

				if (IsDown && (u32) Message.wParam == VK_F1) {
					WindowsToggleFullscreen();
				} else {
					// NOTE(Evan): We intercept numeric VK codes here because Windows is a PITA.
					if (IsDown && ThisFrame->dKeys[PloreKey_Ctrl]) {// && (Message.wParam >= 0x30 && Message.wParam <= 0x39)) {
						WindowsPushTextInput(ThisFrame, Message);
					} else {
						WPARAM C = Message.wParam;
						TranslateMessage(&Message);
						DispatchMessage(&Message);
					}
				}
	        } break;

	        // TODO(Evan): Mouse wheel!
	        case WM_MOUSEMOVE: {
	            ThisFrame->MouseP = (v2) {
					.X = GET_X_LPARAM(Message.lParam),
					.Y = GET_Y_LPARAM(Message.lParam),
				};

	        } break;

			case WM_MOUSEWHEEL: {
				ThisFrame->ScrollDirection = SignOf(GET_WHEEL_DELTA_WPARAM(Message.wParam));
			} break;

			case WM_CHAR: {
				WindowsPushTextInput(ThisFrame, Message);
			} break;
			default: {
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			} break;
		}
	}
}

LRESULT CALLBACK WindowsMessagePumpCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
		case WM_SIZE: {
			// NOTE(Evan): "Window Size" throughout the codebase really means "client rectangle", as in, the drawable area.
			DWORD Style = GetWindowLongPtr(hwnd, GWL_STYLE );
			DWORD ExStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
			HMENU Menu = GetMenu(hwnd);

			DWORD NewWidth = LOWORD(lParam);
			DWORD NewHeight = HIWORD(lParam);
			RECT Rect = { 0, 0, NewWidth, NewHeight };

			AdjustWindowRectEx(&Rect, Style, Menu ? TRUE : FALSE, ExStyle);

			SetWindowPos(hwnd, NULL, 0, 0, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_NOZORDER | SWP_NOMOVE );

			if (GlobalWindowsContext) {
				GlobalWindowsContext->Width = NewWidth;
				GlobalWindowsContext->Height = NewHeight;
			}
		} break;

		case WM_CLOSE: {
			WindowsPrintLine("WM_CLOSE");
			GlobalRunning = false;
		}
        default:
        {
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

    }

    return 0;
}

inline FILETIME
WindowsGetFileLastWriteTime(char *Name) {
	FILETIME Result = {0};
	WIN32_FILE_ATTRIBUTE_DATA Data;
	if (GetFileAttributesExA(Name, GetFileExInfoStandard, &Data)) {
		Result = Data.ftLastWriteTime;
	}

	return(Result);
}

typedef struct plore_code {
	FILETIME DLLLastWriteTime;
	HMODULE DLL;
	plore_do_one_frame *DoOneFrame;
	b32 Valid;
} plore_code;

internal plore_code
WindowsLoadPloreCode(char *DLLPath, char *TempDLLPath, char *LockPath) {
	plore_code Result = {0};
	WIN32_FILE_ATTRIBUTE_DATA Ignored;

	b32 BuildLockFileExists = GetFileAttributesExA(LockPath, GetFileExInfoStandard, &Ignored);

	if (!BuildLockFileExists) {
		WindowsPrintLine("Trying to copy build lock file...");
		Result.DLLLastWriteTime = WindowsGetFileLastWriteTime(DLLPath);

		u32 CopyCount = 0;
		while(CopyCount++ <= 10) {
			if (CopyFile(DLLPath, TempDLLPath, false))  break;
			if (GetLastError() == ERROR_FILE_NOT_FOUND) break;
		}

		if (CopyCount > 10) WindowsPrintLine("Copy attempts exceeded 10!");
	} else {
		WindowsPrintLine("Build lock file found.");
	}


	Result.DLL = LoadLibraryA(TempDLLPath);
	if (Result.DLL) {
		WindowsPrintLine("Loaded DLL found at path :: %s", TempDLLPath);
		Result.DoOneFrame = (plore_do_one_frame *) GetProcAddress(Result.DLL, "PloreDoOneFrame");
		WindowsPrintLine("Loaded proc address of procedure PloreDoOneFrame :: %x", Result.DoOneFrame);
		Result.Valid = !!Result.DoOneFrame;
	} else {
		WindowsPrintLine("Could not load DLL found at temporary path :: %s", TempDLLPath);
	}

	return(Result);
}

internal void
WindowsUnloadPloreCode(plore_code PloreCode) {
	if (PloreCode.DLL) {
		FreeLibrary(PloreCode.DLL);
		PloreCode.DLL = NULL;
	}

	PloreCode.DoOneFrame = NULL;
}

internal b64
_WindowsPathPush(char *Buffer, u64 BufferSize, char *Piece, u64 PieceSize, b64 TrailingSlash) {
	u64 TrailingParts = TrailingSlash ? 1 : 0;
	if (PieceSize + TrailingParts > BufferSize) {
		return(false);
	}

	u64 EndOfBuffer = StringLength(Buffer);
	u64 BytesWritten = StringCopy(Piece, Buffer + EndOfBuffer, BufferSize);

	return(true);
}


DWORD WINAPI
WindowsThreadStart(void *Param) {
	plore_thread_context *Context = (plore_thread_context *)Param;
	platform_taskmaster *Taskmaster = Context->Taskmaster;

	for (;;) {
		DWORD WaitResult = WaitForSingleObject(TaskAvailableSemaphore,
											   INFINITE);
		u64 Initial = Taskmaster->TaskCursor;
		i64 MaybeNewCursor = InterlockedCompareExchange64(&Taskmaster->TaskCursor, (Initial + 1) % ArrayCount(Taskmaster->Tasks), Initial);
		if ((u64)Taskmaster->TaskCursor != Initial) {
			platform_task *Task = Taskmaster->Tasks + Initial;
			Assert(!Task->Running);

			platform_task_info TaskInfo = {
				.TaskArena = &Task->Arena,
				.ThreadID = Context->LogicalID,
			};
			Task->Procedure(TaskInfo, Task->Param);
			Task->Running = false;
		}
	}
}

#if 0
typedef struct overlapped_job {
	OVERLAPPED Overlapped;
	HANDLE File;
	b64 Complete;
	plore_path_buffer Path;
	u8 *Buffer;
} overlapped_job;

typedef struct overlapped_job_guy {
	overlapped_job Jobs[64];
	u64 JobCount;
} overlapped_job_guy;

internal void
PushOverlappedTask(overlapped_job_guy *Guy, plore_path_buffer *Path) {
	Assert(Guy->JobCount < ArrayCount(Guy->Jobs));
	overlapped_job *Job = Guy->Jobs + Guy->JobCount++;
	PathCopy(Job->Path, Path);
	Job->Complete = false;
	
	Job->Overlapped = (OVERLAPPED) {
		.hEvent = CreateEventA(0, true, false, 0),
	};
	Job->File = CreateFileA(Path, 
							GENERIC_READ,
							0,
							0,
							OPEN_EXISTING,
							FILE_ATTRIBUTE_OVERLAPPED,
							0,
							0);
	Assert(Job->File != INVALID_HANDLE_VALUE);
}
#endif

int WinMain (
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CmdLine,
    int ShowCommand)
{
#if PLORE_INTERNAL
	DebugConsoleSetup();
#endif

	// NOTE(Evan): Find absolute runtime paths used to load Plore DLL and reload it on re-compilation.
	DWORD BytesRequired = PLORE_MAX_PATH - 30;
	// TODO(Evan): GetModuleDirectory(?)
	plore_path_buffer ExePath = {0};
	DWORD BytesWritten = GetModuleFileName(NULL, ExePath, ArrayCount(ExePath));
	Assert(BytesWritten < BytesRequired); // NOTE(Evan): Requires our .exe to near the top of a root directory.

	char *PloreDLLPathString = "plore.dll";
	char *TempDLLPathString = "data\\plore_temp.dll";
	char *LockPathString = "lock.tmp";

	plore_path_buffer PloreDLLPath = {0}; // "build/plore.dll";
	plore_path_buffer TempDLLPath = {0};  //  = "data/plore_temp.dll";
	plore_path_buffer LockPath = {0};     //  = "build/lock.tmp";


	StringCopy(ExePath, PloreDLLPath, ArrayCount(PloreDLLPath));
	StringCopy(ExePath, TempDLLPath, ArrayCount(TempDLLPath));
	StringCopy(ExePath, LockPath, ArrayCount(LockPath));

	WindowsPathPop(PloreDLLPath, ArrayCount(PloreDLLPath), true);
	WindowsPathPop(TempDLLPath, ArrayCount(TempDLLPath), false);
	WindowsPathPop(LockPath, ArrayCount(LockPath), true);

	WindowsPathPop(TempDLLPath, ArrayCount(TempDLLPath), true);

	_WindowsPathPush(PloreDLLPath, ArrayCount(PloreDLLPath), PloreDLLPathString, StringLength(PloreDLLPathString), false);
	_WindowsPathPush(TempDLLPath, ArrayCount(TempDLLPath), TempDLLPathString, StringLength(TempDLLPathString), false);
	_WindowsPathPush(LockPath, ArrayCount(LockPath), LockPathString, StringLength(LockPathString), false);


    plore_code PloreCode = WindowsLoadPloreCode(PloreDLLPath, TempDLLPath, LockPath);

	SYSTEM_INFO SystemInfo = {0};
	GetSystemInfo(&SystemInfo);


    plore_memory PloreMemory = {
        .PermanentStorage = {
           .Size = Megabytes(512),
        },
		.TaskStorage = {
			.Size = Taskmaster_MaxTasks*Taskmaster_TaskArenaSize+(Taskmaster_MaxTasks*64),
		},
    };

	// NOTE(Evan): Reserve, commit, and zero memory used for all dynamic allocation.
	{
	    PloreMemory.PermanentStorage.Memory = VirtualAlloc(0, PloreMemory.PermanentStorage.Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		MemoryClear(PloreMemory.PermanentStorage.Memory, PloreMemory.PermanentStorage.Size);
	    Assert(PloreMemory.PermanentStorage.Memory);

		// NOTE(Evan): Right now we check if the base pointer is 16-byte aligned, instead of memory_arenas aligning on initialization,
		// as there is no formal initialization for arenas, other then SubArena()
		Assert(((u64)PloreMemory.PermanentStorage.Memory % 16) == 0);
	}
	{
	    PloreMemory.TaskStorage.Memory = VirtualAlloc(0, PloreMemory.TaskStorage.Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		MemoryClear(PloreMemory.TaskStorage.Memory, PloreMemory.TaskStorage.Size);
	    Assert(PloreMemory.TaskStorage.Memory);

		Assert(((u64)PloreMemory.TaskStorage.Memory % 16) == 0);
	}

	// NOTE(Evan): Thread creation.


	platform_taskmaster Taskmaster = {0};
	InitTaskmaster(&PloreMemory.TaskStorage, &Taskmaster);
    platform_api WindowsPlatformAPI = {
		.Taskmaster = &Taskmaster,

        // TODO(Evan): Update these on resize!
        .WindowWidth = DEFAULT_WINDOW_WIDTH,
        .WindowHeight = DEFAULT_WINDOW_HEIGHT,

#if PLORE_INTERNAL
		.DebugAssertHandler  = WindowsDebugAssertHandler,
        .DebugOpenFile       = WindowsDebugOpenFile,
        .DebugReadEntireFile = WindowsDebugReadEntireFile,
        .DebugReadFileSize   = WindowsDebugReadFileSize,
		.DebugCloseFile      = WindowsDebugCloseFile,

        .DebugPrintLine = WindowsPrintLine,
        .DebugPrint     = WindowsDebugPrint,
#endif

		.CreateTextureHandle = WindowsGLCreateTextureHandle,
		.DestroyTextureHandle = WindowsGLDestroyTextureHandle,

		.ShowCursor = WindowsShowCursor,
		.ToggleFullscreen = WindowsToggleFullscreen,

#undef GetCurrentDirectory
#undef SetCurrentDirectory
#undef MoveFile
#undef CreateFile
#undef CreateDirectory
#undef DeleteFile
#undef PathIsDirectory
		.DirectorySizeTaskBegin  = WindowsDirectorySizeTaskBegin,

		.GetDirectoryEntries     = WindowsGetDirectoryEntries,
		.GetCurrentDirectoryPath = WindowsGetCurrentDirectoryPath,
		.SetCurrentDirectory     = WindowsSetCurrentDirectory,
		.PathPop                 = WindowsPathPop,
		.PathIsDirectory         = WindowsPathIsDirectory,
		.PathIsTopLevel          = WindowsPathIsTopLevel,
		.PathJoin                = WindowsPathJoin,

		.CreateFile              = WindowsCreateFile,
		.CreateDirectory         = WindowsCreateDirectory,
		.DeleteFile              = WindowsDeleteFile,
		.MoveFile                = WindowsMoveFile,
		.RenameFile              = WindowsRenameFile,
		.RunShell                = WindowsRunShell,

		.CreateTaskWithMemory   = WindowsCreateTaskWithMemory,
		.StartTaskWithMemory    = WindowsStartTaskWithMemory,
		.PushTask               = WindowsPushTask,
    };

    windows_context WindowsContext = WindowsCreateAndShowOpenGLWindow(Instance);
	GlobalWindowsContext = &WindowsContext;

	windows_timer PreviousTimer = WindowsGetTime();
	f64 TimePreviousInSeconds = 0;

	windows_get_drives_result Drives = WindowsGetDrives();
	Drives;

    while (GlobalRunning) {
		WindowsPlatformAPI.WindowWidth = GlobalWindowsContext->Width;
		WindowsPlatformAPI.WindowHeight = GlobalWindowsContext->Height;

        // Grab any new keyboard / mouse / window events from message queue.
        MSG WindowMessage = {0};

		WindowsProcessMessages(GlobalWindowsContext, &GlobalPloreInput.ThisFrame);

		// Mouse input!
		// TODO(Evan): A more systematic way of handling our window messages vs input?
		{
			//POINT P;
			//GetCursorPos(&P);
			//ScreenToClient(GlobalWindowsContext->Window, &P);
			//GlobalPloreInput.ThisFrame.MouseP = (v2) {
			//	.X = P.x,
			//	.Y = P.y,
			//};
			GlobalPloreInput.ThisFrame.dKeys[PloreKey_MouseLeft] = GetKeyState(VK_LBUTTON) & (1 << 15);
			GlobalPloreInput.ThisFrame.pKeys[PloreKey_MouseLeft] = (GetKeyState(VK_LBUTTON) & (1 << 15)) && !(GlobalPloreInput.LastFrame.dKeys[PloreKey_MouseLeft]);
		}

		windows_timer CurrentTimer = WindowsGetTime();
        f64 TimeNowInSeconds = ((f64) (CurrentTimer.TicksNow - PreviousTimer.TicksNow) / CurrentTimer.Frequency);
		f64 DT = TimeNowInSeconds - TimePreviousInSeconds;
		DT = Clamp(DT, 0, 1.0f / 100.0f); // TODO(Evan): Get desired frame rate!
		GlobalPloreInput.DT = DT;
		GlobalPloreInput.Time = TimeNowInSeconds;

		// TODO(Evan): PloreMemory is a good place to store a DLL reloading flag,
		//             but we may want to store additional transient state that doesn't
		//             make sense there.
		GlobalPloreInput.DLLWasReloaded = false;
		FILETIME DLLCurrentWriteTime = WindowsGetFileLastWriteTime(PloreDLLPath);
		if (CompareFileTime(&DLLCurrentWriteTime, &PloreCode.DLLLastWriteTime) != 0) {
			WindowsPrintLine("Unloading plore DLL.");
			WindowsUnloadPloreCode(PloreCode);
			WindowsPrintLine("Re-loading plore DLL.");
			GlobalPloreInput.DLLWasReloaded = true;
		    PloreCode = WindowsLoadPloreCode(PloreDLLPath, TempDLLPath, LockPath);
		}

        if (PloreCode.DoOneFrame) {
			windows_timer FileCopyBeginTimer = WindowsGetTime();

			plore_frame_result FrameResult = PloreCode.DoOneFrame(&PloreMemory, &GlobalPloreInput, &WindowsPlatformAPI);

			windows_timer FileCopyEndTimer = WindowsGetTime();
			f64 FileCopyTimeInSeconds = ((f64) ((f64)FileCopyEndTimer.TicksNow - (f64)FileCopyBeginTimer.TicksNow) / (f64)FileCopyBeginTimer.Frequency);

			ImmediateBegin(WindowsContext.Width, WindowsContext.Height);
			for (u64 I = 0; I < FrameResult.RenderList->CommandCount; I++) {
				render_command *C = FrameResult.RenderList->Commands + I;
				switch (C->Type) {
					case RenderCommandType_PrimitiveQuad: {
						DrawSquare(C->Quad);
					} break;

					case RenderCommandType_PrimitiveLine: {
						DrawLine(C->Line);
					} break;

					case RenderCommandType_PrimitiveQuarterCircle: {
						DrawQuarterCircle(C->QuarterCircle);
					} break;

					case RenderCommandType_Text: {
						WriteText(FrameResult.RenderList->Font, C->Text);
					} break;

					case RenderCommandType_Scissor: {
						rectangle R = C->Scissor.Rect;
						glScissor(R.P.X-0, R.P.Y-0, R.Span.W+0, R.Span.H+0);
					} break;

					InvalidDefaultCase;
				}
			}


			SwapBuffers(WindowsContext.DeviceContext);

			if (FrameResult.ShouldQuit) GlobalRunning = false;
		}

		GlobalPloreInput.LastFrame = GlobalPloreInput.ThisFrame;
		GlobalPloreInput.ThisFrame.TextInputCount = 0;
		GlobalPloreInput.ThisFrame.ScrollDirection = 0;
		MemoryClear(GlobalPloreInput.ThisFrame.pKeys, sizeof(GlobalPloreInput.ThisFrame.pKeys));
		MemoryClear(GlobalPloreInput.ThisFrame.TextInput, sizeof(GlobalPloreInput.ThisFrame.TextInput));
		MemoryClear(GlobalPloreInput.ThisFrame.cKeys, sizeof(GlobalPloreInput.ThisFrame.cKeys));


		fflush(stdout);
		fflush(stderr);

    }

	WindowsPrintLine("EXITED MAIN LOOP.");

	WindowsUnloadPloreCode(PloreCode);
    // TODO: Other cleanup!
    fclose(stdout);
    fclose(stderr);
    FreeConsole();
    ReleaseDC(WindowsContext.Window, WindowsContext.DeviceContext);
    CloseWindow(WindowsContext.Window);
    DestroyWindow(WindowsContext.Window);
	return 0;
}
