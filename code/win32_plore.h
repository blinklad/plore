#ifndef WIN32_PLORE
#define WIN32_PLORE

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#include <Windows.h> // the kitchen sink
#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM
#include <shlwapi.h> // PathRemoveFileSpecA, PathRemoveBackslash
#include <fcntl.h> // stdin, stdout, _O_TEXT

#if defined(PLORE_INTERNAL)
#include <stdio.h>
#include <stdlib.h> 
#endif

// NOTE(Evan): This constant is used in plore.h, as although the size is platform-specific, there's no platform-specific semantics required,
//             other than the size being known in all translation units at compile-time.
#undef PLORE_MAX_PATH
#define PLORE_MAX_PATH MAX_PATH

typedef struct windows_context {
    HWND Window;
    HDC DeviceContext;
    HGLRC OpenGLContext;
    int32 Width;
    int32 Height;
	b32 CursorIsShowing;
} windows_context;

typedef struct windows_timer {
    uint64 TicksNow;
    uint64 Frequency;
} windows_timer;

LRESULT CALLBACK WindowsMessagePumpCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif
