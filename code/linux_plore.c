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

	int Flags = O_WRONLY | O_CREATE;
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

PLATFORM_DELETE_FILE(LinuxDeleteFile) {
}

PLATFORM_DEBUG_PRINT_LINE(LinuxDebugPrintLine) {
}

PLATFORM_DEBUG_PRINT(LinuxDebugPrint) {
}

PLATFORM_CREATE_TEXTURE_HANDLE(LinuxCreateTextureHandle) {
}

PLATFORM_DESTROY_TEXTURE_HANDLE(LinuxDestroyTextureHandle) {
}

PLATFORM_SHOW_CURSOR(LinuxShowCursor) {
}

PLATFORM_TOGGLE_FULLSCREEN(LinuxToggleFullscreen) {
}

PLATFORM_GET_CURRENT_DIRECTORY(LinuxGetCurrentDirectory) {
}

PLATFORM_GET_CURRENT_DIRECTORY_PATH(LinuxGetCurrentDirectoryPath) {
}

PLATFORM_SET_CURRENT_DIRECTORY(LinuxSetCurrentDirectory) {
}

PLATFORM_PATH_POP(LinuxPathPop) {
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
}

int
main(int ArgCount, char **Args) {
	LinuxContext.xDisplay = XOpenDisplay(NULL);
	if (!LinuxContext.xDisplay) goto Error;

	LinuxContext.xRoot = DefaultRootWindow(LinuxContext.xDisplay);

	GLint GlAttributes[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	XVisualInfo *xVisualInfo = glXChooseVisual(LinuxContext.xDisplay, 0, GlAttributes);
	if (!xVisualInfo) goto Error;

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
	return(-1);
}
