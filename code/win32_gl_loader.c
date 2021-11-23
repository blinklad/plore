#include <Windows.h>
#include <gl/GL.h>
#include "glext.h"
#include "wglext.h"
#include <stddef.h>
#include <stdint.h>


#pragma comment(lib, "opengl32.lib")

typedef char GLchar;
#define GL_DEBUG_SEVERITY_HIGH            0x9146
#define GL_DEBUG_SEVERITY_MEDIUM          0x9147
#define GL_DEBUG_SEVERITY_LOW             0x9148
#define GL_DEBUG_OUTPUT_SYNCHRONOUS       0x8242


// NOTE(Evan):
// https://code.woboq.org/qt5/include/GL/gl.h.html

#define OPENGL_DEBUG_CALLBACK(name) void APIENTRY name (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
typedef OPENGL_DEBUG_CALLBACK(opengl_debug_callback);

#define PLORE_GL_FUNCS \
    PLORE_X(wglCreateContextAttribsARB,        HGLRC, (HDC hDC, HGLRC hshareContext, const int *attribList)) \
    PLORE_X(wglChoosePixelFormatARB,           BOOL,  (HDC hDC, const int *piAttribIList, const float *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats)) \
    PLORE_X(glDebugMessageCallback,            void,  (opengl_debug_callback callback, const void *userParam)) \
    PLORE_X(wglSwapIntervalEXT,                BOOL,  (int interval))

#pragma warning(push) 
#pragma warning(disable : 4028) // C4028 'formal parameter 3 different from declaration'

#define PLORE_X(Name, Return, Args) typedef Return (WINAPI *PFN_ ## Name) Args;
PLORE_GL_FUNCS
#undef PLORE_X


#define PLORE_X(Name, Ret, Args) static PFN_ ## Name Name;
PLORE_GL_FUNCS
#undef PLORE_X

#pragma warning(pop)

