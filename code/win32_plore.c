#include "plore.h"
#include "plore_common.h"
#include "plore_memory.h"
#include "plore_platform.h"

#include "win32_plore.h"
#include "win32_gl_loader.c"

#include "plore_string.h"

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"
#include "plore_gl.c"

global plore_input GlobalPloreInput;
global windows_timer GlobalWindowsTimer;      // TODO(Evan): Timing!
global windows_context *GlobalWindowsContext;


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

PLATFORM_DEBUG_PRINT_LINE(WindowsDebugPrintLine) {
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
    Assert(File.FileSize <= BufferSize);
    Assert(BufferSize < UINT32_MAX);
    
    DWORD BufferSize32 = (DWORD) BufferSize;

    Result.ReadSuccessfully = ReadFile(FileHandle, Buffer, BufferSize32, &BytesRead, NULL);
    Assert(BytesRead == File.FileSize);
    Result.BytesRead = BytesRead;
    Result.Buffer = Buffer;
    
    return(Result);
}

PLATFORM_CREATE_TEXTURE_HANDLE(WindowsGLCreateTextureHandle) {
	platform_texture_handle Result = {
		.Width = Width,
		.Height = Height,
	};
	
	GLuint Handle;
	glGenTextures(1, &Handle);
	Result.Opaque = Handle;
	glBindTexture(GL_TEXTURE_2D, Result.Opaque);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, Result.Width, Result.Height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, Pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	// NOTE(Evan): Can free 'pixels' at this point.
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
	WindowsDebugPrintLine("CursorCount :: %d", CursorCount);
}


// NOTE(Evan): Not part of the platform API.
OPENGL_DEBUG_CALLBACK(WindowsGLDebugMessageCallback)
{
    if (severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM)
    {
        WindowsDebugPrintLine("OpenGL error!... Not sure what, just an error!");
        // oh no
    }
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
            WindowsDebugPrintLine("Could not make context current!!!");
        }

        HMODULE opengl32_dll = LoadLibraryA("opengl32.dll");

        #define PLORE_X(name, ret, args) \
            name = (PFN_ ## name) wglGetProcAddress(#name);\
            if (!name) { \
                name = (PFN_ ## name) GetProcAddress(opengl32_dll, #name);\
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
            WindowsDebugPrintLine("oh no");
        };

        PIXELFORMATDESCRIPTOR SuggestedPixelFormatARB = {0};
        DescribePixelFormat(WindowsContext.DeviceContext, PixelFormatIDARB, sizeof(PIXELFORMATDESCRIPTOR), &SuggestedPixelFormatARB);
        if (!SetPixelFormat(WindowsContext.DeviceContext, PixelFormatIDARB, &SuggestedPixelFormatARB)) {
            WindowsDebugPrintLine("Could not set pixel format!!");
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


LRESULT CALLBACK WindowsMessagePumpCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#define PROCESS_KEY(Name) ThisFrame->##Name##IsDown = IsDown; \
						  ThisFrame->##Name##WasDown = LastFrame->##Name##IsDown; \
						  ThisFrame->##Name##IsPressed = (ThisFrame->##Name##IsDown) && !(ThisFrame->##Name##WasDown); \
						  WindowsDebugPrint("" #Name " is %s", ThisFrame->##Name##IsDown ? "down, " : "not down, "); \
						  WindowsDebugPrint("" #Name " was %s", ThisFrame->##Name##WasDown ? "down, " : "not down, "); \
						  if (ThisFrame->##Name##IsPressed) WindowsDebugPrint("" #Name " is pressed!"); \
						  if (LastFrame->##Name##IsPressed) WindowsDebugPrint("" #Name " was pressed last frame, so it shouldn't be pressed now!");
	
	
	keyboard_and_mouse *ThisFrame = &GlobalPloreInput.ThisFrame;
	keyboard_and_mouse *LastFrame = &GlobalPloreInput.LastFrame;
    switch (uMsg)
    {
        case WM_KEYDOWN: 
        case WM_KEYUP:
        {
            bool32 IsDown = uMsg == WM_KEYDOWN;

            if (wParam == VK_ESCAPE) {
                WindowsDebugPrintLine("Escape pressed");
                PostQuitMessage(0);
            } else if (IsDown && (u32) wParam == VK_F1) {
				WindowsToggleFullscreen();
			} else {
                switch (wParam) {
                    case 'A': {
                        PROCESS_KEY(A);
                    } break;
                    case 'D': {
                        PROCESS_KEY(D);
                    } break;
                    case 'W': {
                        PROCESS_KEY(W);
                    } break;
                    case 'S': {
                        PROCESS_KEY(S);
                    } break;
					
					case 'H': {
                        PROCESS_KEY(H);
                    } break;
	
					case 'J': {
                        PROCESS_KEY(J);
                    } break;
	
                    case 'K': {
                        PROCESS_KEY(K);
                    } break;
					
                    case 'L': {
                        PROCESS_KEY(L);
                    } break;
					
                    case 'T': {
                        PROCESS_KEY(T);
                    } break;
					
                    case VK_RETURN: {
                        PROCESS_KEY(Return);
                    } break;
                    case VK_SPACE: {
                        PROCESS_KEY(Space);
                    } break;
                    case VK_CONTROL: {
                        PROCESS_KEY(Ctrl);
                    } break;
                    case VK_SHIFT: {
                        PROCESS_KEY(Shift);
                    } break;
                    case VK_OEM_MINUS: {
                        PROCESS_KEY(Minus);
                    } break;
                    case VK_OEM_PLUS: {
                        PROCESS_KEY(Plus);
                    } break;
                                   
                    default:
                    WindowsDebugPrint("...but not recorded!");
                }
            }
            WindowsDebugPrintLine("");
            
        } break;
        
		case WM_LBUTTONDOWN: 
		case WM_LBUTTONUP: 
		{
			b32 IsDown = uMsg == WM_LBUTTONDOWN;
			PROCESS_KEY(MouseLeft);
            WindowsDebugPrintLine("");
		} break;
		
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP: 
		{
			b32 IsDown = uMsg == WM_RBUTTONDOWN;
			PROCESS_KEY(MouseRight);
            WindowsDebugPrintLine("");
		} break;
		
		
        case WM_QUIT:
        case WM_CLOSE: 
        { 
            WindowsDebugPrint("Closing!");
        }
        break;
        
        // TODO(Evan): Mouse wheel!
        case WM_MOUSEMOVE: {
            ThisFrame->MouseP = (v2) {
				.X = GET_X_LPARAM(lParam),
				.Y = GET_Y_LPARAM(lParam),
			};
            
        } break;

		
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
		WindowsDebugPrintLine("Trying to copy build lock file...");
		Result.DLLLastWriteTime = WindowsGetFileLastWriteTime(DLLPath);
		
		u32 CopyCount = 0;
		while(CopyCount++ <= 10) {
			if (CopyFile(DLLPath, TempDLLPath, false))  break;
			if (GetLastError() == ERROR_FILE_NOT_FOUND) break;
		}
		
		if (CopyCount > 10) WindowsDebugPrintLine("Copy attempts exceeded 10!");
	} else {
		WindowsDebugPrintLine("Build lock file found.");
	}
	
	
	Result.DLL = LoadLibraryA(TempDLLPath);
	if (Result.DLL) {
		WindowsDebugPrintLine("Loaded DLL found at path :: %s", TempDLLPath);
		Result.DoOneFrame = (plore_do_one_frame *) GetProcAddress(Result.DLL, "PloreDoOneFrame");
		WindowsDebugPrintLine("Loaded proc address of procedure PloreDoOneFrame :: %x", Result.DoOneFrame);
		Result.Valid = !!Result.DoOneFrame;
	} else {
		WindowsDebugPrintLine("Could not load DLL found at temporary path :: %s", TempDLLPath);
	}
	
	return(Result);
}

PLATFORM_GET_CURRENT_DIRECTORY(WindowsGetCurrentDirectory) {
	GetCurrentDirectory(BufferSize, Buffer);
}

PLATFORM_SET_CURRENT_DIRECTORY(WindowsSetCurrentDirectory) {
	b64 Result = SetCurrentDirectory(Name);
	
	return(Result);
}

PLATFORM_POP_PATH_NODE(WindowsPopPathNode) {
	PathRemoveBackslash(Buffer);
	PathRemoveFileSpecA(Buffer);
	if (AddTrailingSlash) {
		u64 Length = StringLength(Buffer);
		Buffer[Length++] = '\\';
		Buffer[Length++] = '\0';
	}
}

//PLATFORM_STRIP_PATH(WindowsStripPath) {
//}


// NOTE(Evan): Directory name should not include trailing '\' nor any '*' or '?' wildcards.
PLATFORM_GET_DIRECTORY_ENTRIES(WindowsGetDirectoryEntries) {
	directory_entry_result Result = {
		.Name = DirectoryName,
		.Buffer = Buffer,
		.Size = Size,
	};
	
	char SearchableDirectoryName[PLORE_MAX_PATH] = {0};
	u64 BytesWritten = CStringCopy(DirectoryName, SearchableDirectoryName, ArrayCount(SearchableDirectoryName));
	u64 SearchBytesWritten = 0;
	char SearchChars[] = {'\\', '*'};
	b64 SearchCanFit = BytesWritten < PLORE_MAX_PATH + sizeof(SearchChars);
	if (!SearchCanFit) {
		return(Result);
	}
	
	b64 IsDirectoryRoot = PathIsRoot(DirectoryName);
	if (!IsDirectoryRoot) {
		SearchableDirectoryName[BytesWritten + SearchBytesWritten++] = '\\';
	} else {
		int BreakHere = 5;
	}
	
	SearchableDirectoryName[BytesWritten + SearchBytesWritten++] = '*';
	SearchableDirectoryName[BytesWritten + SearchBytesWritten++] = '\0';
	
	WIN32_FIND_DATA FindData = {0};
	HANDLE FindHandle = FindFirstFile(SearchableDirectoryName, &FindData);
	u64 IgnoredCount = 0;
	if (FindHandle != INVALID_HANDLE_VALUE) {
		do {
			Assert(Result.Count < Result.Size);
			
			char *FileName = FindData.cFileName;
			if (FileName[0] == '.' || FileName[0] == '$') continue;
			
			plore_file *File = Buffer + Result.Count;
			
			DWORD FlagsToIgnore = FILE_ATTRIBUTE_DEVICE    |
				                  FILE_ATTRIBUTE_ENCRYPTED |
				                  FILE_ATTRIBUTE_HIDDEN    |
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
			} 
			
			u64 BytesWritten = CStringCopy(DirectoryName, File->AbsolutePath, ArrayCount(File->AbsolutePath));
			Assert(BytesWritten < ArrayCount(File->AbsolutePath) - 2); // NOTE(Evan): For trailing '\';
			File->AbsolutePath[BytesWritten++] = '\\';
			CStringCopy(FindData.cFileName, File->AbsolutePath + BytesWritten, ArrayCount(File->AbsolutePath) - BytesWritten);
			CStringCopy(FindData.cFileName, File->Name, ArrayCount(File->Name));
			
			Result.Count++;
			
		} while (FindNextFile(FindHandle, &FindData));
	} else {
		WindowsDebugPrintLine("Could not open directory %s", SearchableDirectoryName);
	}
	
	Result.Succeeded = true;
	return(Result);
}

u8 FontBuffer[1<<21];
u8 TempBitmap[512*512];

internal plore_font 
FontInit(void)
{
	plore_font Result = {
		.Height = 32.0f,
	};
	platform_readable_file FontFile = WindowsDebugOpenFile("data/fonts/Inconsolata-Light.ttf");
	platform_read_file_result TheFont = WindowsDebugReadEntireFile(FontFile, FontBuffer, ArrayCount(FontBuffer));
	Assert(TheFont.ReadSuccessfully);
	
	stbtt_BakeFontBitmap(FontBuffer, 0, Result.Height, TempBitmap, 512, 512, 32, 96, Result.Data); // no guarantee this fits!
	// can free ttf_buffer at this point
	Result.Handle = WindowsGLCreateTextureHandle(TempBitmap, 512, 512);
	
	
	WindowsDebugCloseFile(FontFile);
	
	return(Result);
}

plore_font GlobalFontHandle;

internal void
WindowsUnloadPloreCode(plore_code PloreCode) {
	if (PloreCode.DLL) {
		FreeLibrary(PloreCode.DLL);
		PloreCode.DLL = NULL;
	}
	
	PloreCode.DoOneFrame = NULL;
}

internal b64
WindowsPushPath(char *Buffer, u64 BufferSize, char *Piece, u64 PieceSize, b64 TrailingSlash) {
	u64 TrailingParts = TrailingSlash ? 1 : 0;
	if (PieceSize + TrailingParts > BufferSize) {
		return(false);
	}
	
	u64 EndOfBuffer = StringLength(Buffer);
	u64 BytesWritten = CStringCopy(Piece, Buffer + EndOfBuffer, BufferSize);
	
	return(true);
}

int WinMain (
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CmdLine,
    int ShowCommand)
{
#if PLORE_INTERNAL 
	DebugConsoleSetup();
#endif
    
	DWORD BytesRequired = PLORE_MAX_PATH - 30;
	// TODO(Evan): GetModuleDirectory(?)
	char ExePath[PLORE_MAX_PATH] = {0}; 
	DWORD BytesWritten = GetModuleFileName(NULL, ExePath, ArrayCount(ExePath));
	Assert(BytesWritten < BytesRequired); // NOTE(Evan): Requires our .exe to near the top of a root directory.
	
	char *PloreDLLPathString = "plore.dll";
	char *TempDLLPathString = "data\\plore_temp.dll";
	char *LockPathString = "lock.tmp";
	
	char PloreDLLPath[PLORE_MAX_PATH] = {0}; // "build/plore.dll";
	char TempDLLPath[PLORE_MAX_PATH] = {0};  //  = "data/plore_temp.dll";
	char LockPath[PLORE_MAX_PATH] = {0};     //  = "build/lock.tmp";
	
	
	CStringCopy(ExePath, PloreDLLPath, ArrayCount(PloreDLLPath));
	CStringCopy(ExePath, TempDLLPath, ArrayCount(TempDLLPath));
	CStringCopy(ExePath, LockPath, ArrayCount(LockPath));
	
	WindowsPopPathNode(PloreDLLPath, ArrayCount(PloreDLLPath), true);
	WindowsPopPathNode(TempDLLPath, ArrayCount(TempDLLPath), false);
	WindowsPopPathNode(LockPath, ArrayCount(LockPath), true);
	
	WindowsPopPathNode(TempDLLPath, ArrayCount(TempDLLPath), true);
	
	WindowsPushPath(PloreDLLPath, ArrayCount(PloreDLLPath), PloreDLLPathString, StringLength(PloreDLLPathString), false);
	WindowsPushPath(TempDLLPath, ArrayCount(TempDLLPath), TempDLLPathString, StringLength(TempDLLPathString), false);
	WindowsPushPath(LockPath, ArrayCount(LockPath), LockPathString, StringLength(LockPathString), false);
	
    plore_code PloreCode = WindowsLoadPloreCode(PloreDLLPath, TempDLLPath, LockPath);
	
    plore_memory PloreMemory = {
        .PermanentStorage ={
           .Size = Megabytes(128),
        },
    };
	
    platform_api WindowsPlatformAPI = {
        // TODO(Evan): Update these on resize!
        .WindowWidth = DEFAULT_WINDOW_WIDTH,
        .WindowHeight = DEFAULT_WINDOW_HEIGHT,
		
		#if PLORE_INTERNAL
        .DebugOpenFile = WindowsDebugOpenFile,
        .DebugReadEntireFile = WindowsDebugReadEntireFile,
        .DebugPrintLine = WindowsDebugPrintLine,
        .DebugPrint = WindowsDebugPrint,
		#endif
		
		.CreateTextureHandle = WindowsGLCreateTextureHandle,
		.DestroyTextureHandle = WindowsGLDestroyTextureHandle,
		.ShowCursor = WindowsShowCursor,
		.ToggleFullscreen = WindowsToggleFullscreen,
		
		
#undef GetCurrentDirectory // @Hack
#undef SetCurrentDirectory // @Hack
		.GetDirectoryEntries = WindowsGetDirectoryEntries,
		.GetCurrentDirectory = WindowsGetCurrentDirectory,
		.SetCurrentDirectory = WindowsSetCurrentDirectory,
		.PopPathNode = WindowsPopPathNode,
    };
    
    PloreMemory.PermanentStorage.Memory = VirtualAlloc(0, PloreMemory.PermanentStorage.Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	ClearMemory(PloreMemory.PermanentStorage.Memory, PloreMemory.PermanentStorage.Size);
    Assert(PloreMemory.PermanentStorage.Memory);

    windows_context WindowsContext = WindowsCreateAndShowOpenGLWindow(Instance);
	GlobalWindowsContext = &WindowsContext;
	
    BOOL ReceivedQuitMessage = false;
    
	windows_timer PreviousTimer = WindowsGetTime();
	f64 TimePreviousInSeconds = 0;
	
	plore_font MyFont = FontInit();
	
    while (!ReceivedQuitMessage) {
		WindowsPlatformAPI.WindowWidth = GlobalWindowsContext->Width;
		WindowsPlatformAPI.WindowHeight = GlobalWindowsContext->Height;
		
        /* Grab any new keyboard / mouse / window events from message queue. */
        MSG WindowMessage = {0};
        while (PeekMessage(&WindowMessage, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&WindowMessage);
            DispatchMessage(&WindowMessage);

            if (WindowMessage.message == WM_QUIT) {
                WindowsDebugPrint("Quit message!");
                ReceivedQuitMessage = true;
                break;
            }
        }
		
		// Mouse input!
		// TODO(Evan): A more systematic way of handling our window messages vs input?
		{
			POINT P;
			GetCursorPos(&P);
			ScreenToClient(GlobalWindowsContext->Window, &P);
			GlobalPloreInput.ThisFrame.MouseP = (v2) {
				.X = P.x,
				.Y = P.y,
			};
			GlobalPloreInput.ThisFrame.MouseLeftIsDown    = GetKeyState(VK_LBUTTON) & (1 << 15);
			GlobalPloreInput.ThisFrame.MouseLeftIsPressed = GlobalPloreInput.ThisFrame.MouseLeftIsDown && !(GlobalPloreInput.LastFrame.MouseLeftIsDown);
		}		
        if (ReceivedQuitMessage) break;

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
			WindowsDebugPrintLine("Unloading plore DLL.");
			WindowsUnloadPloreCode(PloreCode);
			WindowsDebugPrintLine("Re-loading plore DLL.");
			GlobalPloreInput.DLLWasReloaded = true;
		    PloreCode = WindowsLoadPloreCode(PloreDLLPath, TempDLLPath, LockPath);
		}
		
        if (PloreCode.DoOneFrame) {
			windows_timer FileCopyBeginTimer = WindowsGetTime();
			
			plore_render_list RenderList = PloreCode.DoOneFrame(&PloreMemory, &GlobalPloreInput, &WindowsPlatformAPI);
			
			windows_timer FileCopyEndTimer = WindowsGetTime();
			f64 FileCopyTimeInSeconds = ((f64) ((f64)FileCopyEndTimer.TicksNow - (f64)FileCopyBeginTimer.TicksNow) / (f64)FileCopyBeginTimer.Frequency);
			
//			WindowsDebugPrintLine("File timing :: %f seconds", FileCopyTimeInSeconds);
			
			ImmediateBegin(WindowsContext.Width, WindowsContext.Height, MyFont);
			for (u64 I = 0; I < RenderList.QuadCount; I++) {
				DrawSquare(RenderList.Quads[I]);
			}
			
			for (u64 I = 0; I < RenderList.TextCount; I++) {
				WriteText(RenderList.Text[I]);
			}
			
			
			SwapBuffers(WindowsContext.DeviceContext);
		}
		
		GlobalPloreInput.LastFrame = GlobalPloreInput.ThisFrame;
#define PLORE_X(Name) GlobalPloreInput.ThisFrame.##Name##IsDown = GlobalPloreInput.LastFrame.##Name##IsDown || GlobalPloreInput.ThisFrame.##Name##IsPressed;
		GlobalPloreInput.ThisFrame = (keyboard_and_mouse) {0};
		PLORE_KEYBOARD_AND_MOUSE;
#undef PLORE_X
		
		fflush(stdout);
		fflush(stderr);
    }

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