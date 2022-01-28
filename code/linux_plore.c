#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include <stdio.h>

#include "plore.h"
#include "plore_platform.h"

#include "plore_memory.h"
#include "plore_memory.c"

#include "plore_gl.h"
#include "plore_gl.c"
#define LinuxCreateTextureHandle  GLCreateTextureHandle
#define LinuxDestroyTextureHandle GLDestroyTextureHandle


typedef struct linux_context {
	XWindowAttributes xWindowAttributes;
	Display *xDisplay;
	Window xRoot;
	Window xWindow;
	GLXContext xGLContext;
} linux_context;

global linux_context LinuxContext;

// CLEANUP(Evan): POSIX i/o for all non-debug functions.
PLATFORM_DEBUG_OPEN_FILE(LinuxDebugOpenFile) {
	platform_readable_file Result = {
		.Opaque = fopen(Path, "r"),
	};
	if (Result.Opaque) {
		Result.OpenedSuccessfully = true;
		fseek(Result.Opaque, 0, SEEK_END);
		Result.FileSize = ftell(Result.Opaque);
		fseek(Result.Opaque, 0, SEEK_SET);
	}

	return(Result);
}

PLATFORM_DEBUG_CLOSE_FILE(LinuxDebugCloseFile) {
	Assert(File.Opaque);
	fclose(File.Opaque);
}

PLATFORM_DEBUG_READ_ENTIRE_FILE(LinuxDebugReadEntireFile) {
	platform_read_file_result Result = {0};

	Assert(File.Opaque);
	Assert(File.FileSize);

	if (File.Opaque && BufferSize && File.FileSize == BufferSize) {
		Result.BytesRead = fread(Buffer, 1, BufferSize, File.Opaque);
		Result.ReadSuccessfully = true;
	}

	return(Result);
}

PLATFORM_DEBUG_READ_FILE_SIZE(LinuxDebugReadFileSize) {
	platform_read_file_result Result = {0};

	Assert(File.Opaque);
	Assert(File.FileSize);

	if (File.Opaque && BufferSize && File.FileSize) {
		Result.BytesRead = fread(Buffer, 1, BufferSize, File.Opaque);
		Result.ReadSuccessfully = true;
	}

	return(Result);
}


#include <sys/stat.h>
#include <fcntl.h>
PLATFORM_CREATE_FILE(LinuxCreateFile) {
	b64 Result = false;

	int Flags = O_WRONLY | O_CREAT;
	if (OverwriteExisting) Flags |= O_TRUNC;

	int FD = open(Path, Flags, S_IRUSR);
	if (FD > 0) {
		if (close(FD) == 0) Result = true;
	}

	return(Result);
}

PLATFORM_CREATE_DIRECTORY(LinuxCreateDirectory) {
	b64 Result = mkdir(Path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0;
	return(Result);
}

// NOTE(Evan): NFTW is _not_ thread safe... ffs.
// So, we will have to use the fts_open family of functions because POSIX is garbage and doesn't have a nnftw.
#define __USE_XOPEN_EXTENDED
#include <ftw.h>
struct FTW;
#define FILE_TREE_WALK_CALLBACK(name) int (name)(const char *Path, const struct stat *Stat, int Info, struct FTW *Context)

global b64 RecursiveDelete = false;
global b64 PermanentDelete = false;
FILE_TREE_WALK_CALLBACK(LinuxDeleteFileCallback) {
	b64 Error = false;
	if (Info != FTW_NS) {
		// Do delete.
	} else {
		Error = true;
	}
	return(Error);
}

PLATFORM_DELETE_FILE(LinuxDeleteFile) {
	Assert(Path);
	b64 Result = false;
	if (Path) {
		RecursiveDelete = Desc.Recursive;
		PermanentDelete = Desc.PermanentDelete;

		Result = nftw(Path,
				      LinuxDeleteFileCallback,
					  4096, // Some large number of file descriptors. The documentation is not clear on how ntfw reuses these.
					  FTW_DEPTH | FTW_PHYS);
	}

	return(Result);
}

PLATFORM_DEBUG_PRINT_LINE(LinuxDebugPrintLine) {
    va_list Args;
    va_start(Args, Format);
    local char Buffer[256];
    stbsp_vsnprintf(Buffer, 256, Format, Args);
    fprintf(stdout, Buffer);
    va_end(Args);
}

PLATFORM_DEBUG_PRINT(LinuxDebugPrint) {
    va_list Args;
    va_start(Args, Format);
    local char Buffer[256];
    stbsp_vsnprintf(Buffer, 256, Format, Args);
    fprintf(stdout, Buffer);
    fprintf(stdout, "\n");
    va_end(Args);
}

PLATFORM_SHOW_CURSOR(LinuxShowCursor) {
	Debugger;
}

PLATFORM_TOGGLE_FULLSCREEN(LinuxToggleFullscreen) {
	Debugger;
}

#include <unistd.h>
#include <libgen.h>

PLATFORM_GET_CURRENT_DIRECTORY_PATH(LinuxGetCurrentDirectoryPath) {
	// NOTE(Evan): basename modifies the path you provide it in-place, instead of providing a byte offset
	// within the path. UGH.
	plore_path Result = {0};

	getcwd(Result.Absolute, ArrayCount(Result.Absolute));
	PathCopy(Result.Absolute, Result.FilePart);
	basename(Result.FilePart);

	return(Result);
}

PLATFORM_SET_CURRENT_DIRECTORY(LinuxSetCurrentDirectory) {
	b64 Result = chdir(Path);
	return(Result);
}

typedef struct find_trailing_slash_result {
	char *Base;
	char *Slash;
	u64 Offset;
	b64 Found;
} find_trailing_slash_result;

internal find_trailing_slash_result
FindTrailingSlash(char *S, u64 Length) {
	b64 Found = false;
	u64 Offset = Length;
	char *C = S + --Offset;

	while (Offset && !Found) {
		if (*C == '/') Found = true;
		else C -= 1, Offset -= 1;
	}

	return((find_trailing_slash_result) {
			.Base = S,
			.Slash = C,
			.Offset = Offset,
			.Found = Found,
			});
}

internal b64
HasTrailingSlash(char *String, u64 Length) {
	b64 Result = String[Length-1] == '/';
	return(Result);
}

PLATFORM_PATH_POP(LinuxPathPop) {
	platform_path_pop_result Result = {
		.AbsolutePath = Buffer,
	};

	u64 Length = StringLength(Buffer);
	if (HasTrailingSlash(Buffer, Length)) Length -= 1;

	find_trailing_slash_result Slash = FindTrailingSlash(Buffer, Length);
	if (Slash.Found) {
		Result.DidRemoveSomething = true;

		if (AddTrailingSlash) {
			Slash.Base[Slash.Offset+1] = '\0';
		} else {
			Slash.Base[Slash.Offset] = '\0';
		}

		Length = StringLength(Buffer);
		if (HasTrailingSlash(Buffer, Length)) Length -= 1;

		find_trailing_slash_result Slash = FindTrailingSlash(Buffer, Length);
		if (Slash.Found) {
			Result.FilePart = Slash.Slash+1;
		}
	}

	return(Result);
}

PLATFORM_PATH_IS_DIRECTORY(LinuxPathIsDirectory) {
	b64 Result = false;
	struct stat Stat = {0};
	stat(Buffer, &Stat);

	Result = ((Stat.st_mode & S_IFMT) == S_IFDIR);
	return(Result);
}

PLATFORM_PATH_IS_TOP_LEVEL(LinuxPathIsTopLevel) {
	b64 Result = Buffer[0] == '/' && !Buffer[1];
	return(Result);
}

PLATFORM_PATH_JOIN(LinuxPathJoin) {
	b64 DidJoin = false;

	u64 LengthA = StringLength(First);
	u64 LengthB = StringLength(Second);

	u64 TotalLength = LengthA + LengthB + 1;
	Assert(TotalLength < PLORE_MAX_PATH);
	if (TotalLength < PLORE_MAX_PATH) {
		u64 BytesWritten = StringCopy(First, Buffer, PLORE_MAX_PATH);
		Buffer[BytesWritten++] = '/';
		StringCopy(Second, Buffer+BytesWritten, PLORE_MAX_PATH-BytesWritten);
		DidJoin = true;
	}

	return(DidJoin);
}

#include <sys/types.h>
#include <dirent.h>
PLATFORM_GET_DIRECTORY_ENTRIES(LinuxGetDirectoryEntries) {
	directory_entry_result Result = {
		.Name = DirectoryName,
		.Buffer = Buffer,
		.Size = Size,
	};

	DIR *Directory = opendir(DirectoryName);
	if (Directory) {
		struct dirent *Entry = readdir(Directory);
		while (Entry) {
			Entry = readdir(Directory);
			if (Entry) {
				if (Result.Count == Result.Size) break;

				plore_file *F = Result.Buffer + Result.Count++;

				PathCopy(Entry->d_name, F->Path.FilePart);
				b64 JoinedOkay = LinuxPathJoin(F->Path.Absolute, DirectoryName, Entry->d_name);
				Assert(JoinedOkay);

				if (Entry->d_type == DT_REG) {
					F->Type = PloreFileNode_File;
				} else if (Entry->d_type == DT_DIR) {
					F->Type = PloreFileNode_Directory;
				} else {
					LinuxDebugPrintLine("Unhandled file type!");
				}

			}

		}

		Result.Succeeded = closedir(Directory) == 0;

		for (u64 E = 0; E < Result.Count; E++) {
			plore_file *F = Result.Buffer + E;
			struct stat Stat = {0};
			stat(F->Path.Absolute, &Stat);
			F->LastModification.T = Stat.st_mtime;
			F->Bytes = Stat.st_size;
		}

	}

	return(Result);
}

PLATFORM_DIRECTORY_SIZE_TASK_BEGIN(LinuxDirectorySizeTaskBegin) {
}

PLATFORM_MOVE_FILE(LinuxMoveFile) {
	b64 Result = false;
	Debugger;
	return(Result);
}

PLATFORM_RENAME_FILE(LinuxRenameFile) {
	b64 Result = false;
	Debugger;
	return(Result);
}

PLATFORM_RUN_SHELL(LinuxRunShell) {
	b64 Result = false;
	Debugger;
	return(Result);
}

typedef struct platform_taskmaster {
	u64 __Dummy;
} platform_taskmaster;

PLATFORM_CREATE_TASK_WITH_MEMORY(LinuxCreateTaskWithMemory) {
	task_with_memory_handle Result = {0};
	Debugger;
	return(Result);
}

PLATFORM_START_TASK_WITH_MEMORY(LinuxStartTaskWithMemory) {
}

PLATFORM_PUSH_TASK(LinuxPushTask) {
}

PLATFORM_DEBUG_ASSERT_HANDLER(LinuxDebugAssertHandler) {
	Debugger;
	return(0);
}

internal u64
LinuxGetTimeInMS()
{
	struct timespec Time;
	clock_gettime(CLOCK_MONOTONIC, &Time);

	time_t SecondsToMS = Time.tv_sec * 1000;
	long NanoSecondsToMS = Time.tv_nsec / 1000000;

	return(SecondsToMS + NanoSecondsToMS);
}

#include <dlfcn.h>
#include <sys/mman.h>

#define GOTO_ERROR_IF_FAILED(Predicate, Message) if (!Predicate) { ErrorMessage = Message; goto Error; }
int
main(int ArgCount, char **Args) {
	plore_path MyPath = LinuxGetCurrentDirectoryPath();

	char *ErrorMessage = 0;

	LinuxContext.xDisplay = XOpenDisplay(NULL);
	GOTO_ERROR_IF_FAILED(LinuxContext.xDisplay, "XOpenDisplay")

	LinuxContext.xRoot = DefaultRootWindow(LinuxContext.xDisplay);

	GLint GlAttributes[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	XVisualInfo *xVisualInfo = glXChooseVisual(LinuxContext.xDisplay, 0, GlAttributes);
	GOTO_ERROR_IF_FAILED(xVisualInfo, "glXChooseVisual");

	Colormap xColorMap = XCreateColormap(LinuxContext.xDisplay, LinuxContext.xRoot, xVisualInfo->visual, AllocNone);
	XSetWindowAttributes xWindowAttributeDesc = {
		.colormap = xColorMap,
		.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask,
	};
	LinuxContext.xWindow = XCreateWindow(LinuxContext.xDisplay,
			LinuxContext.xRoot,
			0,
			0,
			1920,
			1080,
			0,
			xVisualInfo->depth,
			InputOutput,
			xVisualInfo->visual,
			CWColormap | CWEventMask,
			&xWindowAttributeDesc);

	XMapWindow(LinuxContext.xDisplay, LinuxContext.xWindow);
	XStoreName(LinuxContext.xDisplay, LinuxContext.xWindow, "Plore");

	LinuxContext.xGLContext = glXCreateContext(LinuxContext.xDisplay, xVisualInfo, NULL, GL_TRUE);
	glXMakeCurrent(LinuxContext.xDisplay, LinuxContext.xWindow, LinuxContext.xGLContext);

	platform_taskmaster LinuxTaskmaster = {0};

#define PREFIX(Function) .Function = Linux##Function,
	platform_api LinuxPlatformAPI = {
		.Taskmaster = &LinuxTaskmaster,
		.WindowDimensions = V2(1920, 1080),
		PREFIX(DebugAssertHandler)
		PREFIX(CreateTextureHandle)
		PREFIX(DestroyTextureHandle)
		PREFIX(ShowCursor)
		PREFIX(ToggleFullscreen)
		PREFIX(DebugOpenFile)
		PREFIX(DebugReadEntireFile)
		PREFIX(DebugReadFileSize)
		PREFIX(DebugCloseFile)
		PREFIX(DebugPrintLine)
		PREFIX(DebugPrint)
		PREFIX(DirectorySizeTaskBegin)
		PREFIX(GetDirectoryEntries)
		PREFIX(GetCurrentDirectoryPath)
		PREFIX(SetCurrentDirectory)
		PREFIX(PathPop)
		PREFIX(PathJoin)
		PREFIX(PathIsDirectory)
		PREFIX(PathIsTopLevel)
		PREFIX(CreateFile)
		PREFIX(CreateDirectory)
		PREFIX(MoveFile)
		PREFIX(RenameFile)
		PREFIX(DeleteFile)
		PREFIX(RunShell)
		PREFIX(CreateTaskWithMemory)
		PREFIX(StartTaskWithMemory)
		PREFIX(PushTask)
	};

#define StoreSize Megabytes(512)

	// Summary notes on MAP_ANONYMOUS, initialization and args:
	// "The mapping is not backed by any file; its contents are initialized to zero.
    //  ...some implementations require fd to be -1 if MAP_ANONYMOUS (or MAP_ANON) is specified.
    //  ...The offset argument should be zero."
	// https://man7.org/linux/man-pages/man2/mmap.2.html
	//
	// TODO(Evan): Should we use MAP_POPULATE to pre-fault the allocated pages?
	//
	plore_memory PloreMemory = {
        .PermanentStorage = {
           .Size = StoreSize,
		   .Memory = mmap(0, StoreSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0),
        },
		// TODO(Evan): TaskStorage.
	};
	Assert(PloreMemory.PermanentStorage.Memory != MAP_FAILED);

	plore_path_buffer LibraryPath = {0};
	MemoryCopy(MyPath.Absolute, LibraryPath, PLORE_MAX_PATH);
	char *Library = StringConcatenate(LibraryPath, PLORE_MAX_PATH, "/build/plore.so");
	void *LibraryHandle = dlopen(Library, RTLD_NOW);
	Assert(LibraryHandle);
	plore_do_one_frame *PloreDoOneFrame = dlsym(LibraryHandle, "PloreDoOneFrame");
	Assert(PloreDoOneFrame);


	u64 TicksPrev = LinuxGetTimeInMS();
	plore_input PloreInput = {0};
	XEvent xEvent = {0};

	for (;;) {
		u64 TicksNow = LinuxGetTimeInMS();
		u64 TicksDT = TicksNow - TicksPrev;
		f32 DT = (f32)TicksDT / 1000.0f;

		if (XPending(LinuxContext.xDisplay)) {
			XNextEvent(LinuxContext.xDisplay, &xEvent);
			switch (xEvent.type) {
				case ConfigureNotify: {
					XConfigureEvent Configure = xEvent.xconfigure;

					if (Configure.width  != LinuxPlatformAPI.WindowWidth ||
						Configure.height != LinuxPlatformAPI.WindowHeight) {
						LinuxPlatformAPI.WindowWidth = Configure.width;
						LinuxPlatformAPI.WindowHeight = Configure.height;
						XGetWindowAttributes(LinuxContext.xDisplay, LinuxContext.xWindow, &LinuxContext.xWindowAttributes);
					}
				} break;
				// TODO(Evan): Keyboard/mouse input
				case KeyRelease:
				case KeyPress: {
					int Foo = 5;
					b64 IsPressed = xEvent.type == KeyPress;
					b64 IsUnpressed = xEvent.type == KeyRelease;
					KeySym Keycode = XkbKeycodeToKeysym(LinuxContext.xDisplay, xEvent.xkey.keycode, 0, xEvent.xkey.state);
				} break;
			}
		}

		plore_frame_result FrameResult = PloreDoOneFrame(&PloreMemory, &PloreInput, &LinuxPlatformAPI);

		ImmediateBegin(LinuxContext.xWindowAttributes.width, LinuxContext.xWindowAttributes.height);
		DoRender(FrameResult.RenderList);
		glXSwapBuffers(LinuxContext.xDisplay, LinuxContext.xWindow);

		PloreInput.LastFrame = PloreInput.ThisFrame;
		TicksPrev = TicksNow;
	}

	glXMakeCurrent(LinuxContext.xDisplay, None, NULL);
	glXDestroyContext(LinuxContext.xDisplay, LinuxContext.xGLContext);
	XDestroyWindow(LinuxContext.xDisplay, LinuxContext.xWindow);
	XCloseDisplay(LinuxContext.xDisplay);

	i64 ExitCode = 0;

Error:
	if (ErrorMessage) {
		LinuxDebugPrintLine(ErrorMessage);
		ExitCode = -1;
	}
	return(ExitCode);
}
