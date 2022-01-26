#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include <stdio.h>

#include "plore.h"
#include "plore_platform.h"
#include "plore_gl.h"
#include "plore_gl.c"

#include "plore_memory.h"
#include "plore_memory.c"


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

		Result = ntfw(Path,
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

// CLEANUP(Evan): Renderer bits.
PLATFORM_CREATE_TEXTURE_HANDLE(LinuxCreateTextureHandle) {
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

PLATFORM_DESTROY_TEXTURE_HANDLE(LinuxDestroyTextureHandle) {
	GLuint Opaque = (GLuint) Texture.Opaque;
	glDeleteTextures(1, &Opaque);
}

PLATFORM_SHOW_CURSOR(LinuxShowCursor) {
	Debugger;
}

PLATFORM_TOGGLE_FULLSCREEN(LinuxToggleFullscreen) {
	Debugger;
}

#define _POSIX_SOURCE
#include <unistd.h>
#include <libgen.h>

PLATFORM_GET_CURRENT_DIRECTORY(LinuxGetCurrentDirectoryPath) {
	// NOTE(Evan): basename modifies the path you provide it in-place, instead of providing a byte offset
	// within the path. UGH.
	plore_path Result = {0};

	getcwd(Result.Path.Absolute, ArrayCount(Result.Path.Absolute));
	PathCopy(Result.Path.Absolute, Result.Path.FilePart);
	basename(Result.Path.FilePart, ArrayCount(Result.Path.FilePart));

	return(Result);
}

PLATFORM_SET_CURRENT_DIRECTORY(LinuxSetCurrentDirectory) {
	b64 Result = chdir(Path);
	return(Result);
}

PLATFORM_PATH_POP(LinuxPathPop) {
	platform_path_pop_result Result = {
		.AbsolutePath = Buffer,
	};
	u64 Length = StringLength(Buffer);
	if (Buffer[Length] != '/') {
		Result.DidRemoveSomething = false;
	} else {
		u64 L = Length;
		char *C = Buffer+Length;
		while (L--) {
			if (*--C == '/') break;
		}
		Buffer[Length-L] = '\0';
	}

	plore_path_buffer Temp = {0};
	StringCopy(Buffer, Temp, ArrayCount(Temp));
	basename(Temp, ArrayCount(Temp));
	Result.FilePart = Buffer + Substring(Buffer, Temp).Index;

	return(Result);
}

PLATFORM_PATH_PUSH(LinuxPathPush) {
}

PLATFORM_PATH_IS_DIRECTORY(LinuxPathIsDirectory) {
}

PLATFORM_PATH_IS_TOP_LEVEL(LinuxPathIsTopLevel) {
}

PLATFORM_PATH_JOIN(LinuxPathJoin) {
}

PLATFORM_GET_DIRECTORY_ENTRIES(LinuxGetDirectoryEntries) {
}

PLATFORM_DIRECTORY_SIZE_TASK_BEGIN(LinuxDirectorySizeTaskBegin) {
}

PLATFORM_MOVE_FILE(LinuxMoveFile) {
}

PLATFORM_RENAME_FILE(LinuxRenameFile) {
}

PLATFORM_RUN_SHELL(LinuxRunShell) {
}

PLATFORM_CREATE_TASK_WITH_MEMORY(LinuxCreateTaskWithMemory) {
}

PLATFORM_START_TASK_WITH_MEMORY(LinuxStartTaskWithMemory) {
}

PLATFORM_PUSH_TASK(LinuxPushTask) {
}

PLATFORM_DEBUG_ASSERT_HANDLER(LinuxDebugAssertHandler) {
	Debugger;
}

#define GOTO_ERROR_IF_FAILED(Predicate, Message) if (!Predicate) { ErrorMessage = Message; goto Error; }

int
main(int ArgCount, char **Args) {
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
		.event_mask = ExposureMask | KeyPressMask,
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

	platform_api LinuxPlatformAPI = {
		0,
		// TODO(Evan): Function pointers.
	};

	XEvent xEvent = {0};
	for (;;) {
		XNextEvent(LinuxContext.xDisplay, &xEvent);
		if (xEvent.type == Expose) {
			XGetWindowAttributes(LinuxContext.xDisplay, LinuxContext.xWindow, &LinuxContext.xWindowAttributes);
			ImmediateBegin(LinuxContext.xWindowAttributes.width, LinuxContext.xWindowAttributes.height);

			DrawSquare((render_quad) {
					.Rect = {
						.P = V2(1920/2, 1080/2),
						.Span = V2(512, 512),
					},
					.Colour = WHITE_V4,
					});

			glXSwapBuffers(LinuxContext.xDisplay, LinuxContext.xWindow);
		} else if (xEvent.type == KeyPress) {
		}
	}

	glXMakeCurrent(LinuxContext.xDisplay, None, NULL);
	glXDestroyContext(LinuxContext.xDisplay, LinuxContext.xGLContext);
	XDestroyWindow(LinuxContext.xDisplay, LinuxContext.xWindow);
	XCloseDisplay(LinuxContext.xDisplay);

Error:
	if (ErrorMessage) {
		LinuxDebugPrintLine(ErrorMessage);
	}
	return(-1);
}
