#include "plore.h"
#include "plore_common.h"
#include "plore_memory.h"
#include "plore_platform.h"

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"

#include "win32_plore.h"
#include "win32_gl_loader.c"
#include "plore_gl.c"

global plore_input GlobalPloreInput;
global windows_timer GlobalWindowsTimer;      // TODO(Evan): Timing!
global windows_context *GlobalWindowsContext;

internal void
WindowsDebugPrint(const char *Format, ...) {
    va_list Args;
    va_start(Args, Format);
    static char Buffer[256];
    vsnprintf(Buffer, 256, Format, Args);
    fprintf(stdout, Buffer);
    va_end(Args);
    OutputDebugStringA(Buffer);
}

internal void
WindowsDebugPrintLine(const char *Format, ...) {
    va_list Args;
    va_start(Args, Format);
    static char Buffer[256];
    vsnprintf(Buffer, 256, Format, Args);
    fprintf(stdout, Buffer);
    fprintf(stdout, "\n");
    va_end(Args);
}

OPENGL_DEBUG_CALLBACK(WindowsGLDebugMessageCallback)
{
    if (severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM)
    {
        WindowsDebugPrintLine("OpenGL error!... Not sure what, just an error!");
        // oh no
    }
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

// NOTE(Evan): Windows decrements a signed counter whenever FALSE is provided, so there will be some weirdness to this.
PLATFORM_SHOW_CURSOR(WindowsShowCursor) {
	GlobalPloreInput.ThisFrame.CursorIsShowing = Show;
	DWORD CursorCount = ShowCursor(Show);
	WindowsDebugPrintLine("CursorCount :: %d", CursorCount);
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
    
	ShowCursor(TRUE);
	
    // 1/0 = Enable/disable vsync, -1 = adaptive sync.
    wglSwapIntervalEXT(1);

    return WindowsContext;
}


LRESULT CALLBACK WindowsMessagePumpCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#define PROCESS_KEY(Name) ThisFrame->##Name##IsDown = IsDown; \
						  ThisFrame->##Name##WasDown = LastFrame->##Name##IsDown; \
						  ThisFrame->##Name##IsPressed = ThisFrame->##Name##IsDown && !ThisFrame->##Name##WasDown; \
						  WindowsDebugPrint("" #Name " is %s!", ThisFrame->##Name##IsDown ? "down" : "not down"); \
						  if (ThisFrame->##Name##IsPressed) WindowsDebugPrint("" #Name " is pressed!");
	
	
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
            }
            else {
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
        
        case WM_QUIT:
        case WM_CLOSE: 
        { 
            WindowsDebugPrint("Closing!");
        }
        break;
        
        // TODO(Evan): Mouse wheel!
        case WM_MOUSEMOVE: {
			b32 IsDown = uMsg == WM_KEYDOWN;
            ThisFrame->MouseX = GET_X_LPARAM(lParam);
            ThisFrame->MouseY = GET_Y_LPARAM(lParam);
            
            switch (uMsg) {
                case MK_LBUTTON: {
                    PROCESS_KEY(MouseLeft);
                } break;

                case MK_RBUTTON: {
                    PROCESS_KEY(MouseRight);
                } break;
            }
        } break;

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

internal void
WindowsGetCurrentDirectory(char *Buffer, u64 BufferSize) {
	GetCurrentDirectory(BufferSize, Buffer);
}

internal void
WindowsPopPathNode(char *Buffer, u64 BufferSize) {
	PathRemoveFileSpecA(Buffer);
}

// NOTE(Evan): Directory name should not include trailing '\' nor any '*' or '?' wildcards.
internal directory_entry_result
WindowsGetEntriesForDirectory(char *DirectoryName, plore_file *Buffer, u64 Size)
{
	directory_entry_result Result = {
		.DirectoryName = DirectoryName,
		.Buffer = Buffer,
		.Size = Size,
	};
	
	char SearchableDirectoryName[PLORE_MAX_PATH] = {0};
	u64 BytesWritten = CStringCopy(DirectoryName, SearchableDirectoryName, ArrayCount(SearchableDirectoryName));
	char SearchChars[] = {'\\', '*'};
	b64 SearchCanFit = BytesWritten < PLORE_MAX_PATH + sizeof(SearchChars);
	if (!SearchCanFit) {
		return(Result);
	}
	SearchableDirectoryName[BytesWritten+0] = '\\';
	SearchableDirectoryName[BytesWritten+1] = '*';
	SearchableDirectoryName[BytesWritten+2] = '\0';
	
	WIN32_FIND_DATA FindData = {0};
	HANDLE FindHandle = FindFirstFile(SearchableDirectoryName, &FindData);
	u64 IgnoredCount = 0;
	if (FindHandle != INVALID_HANDLE_VALUE) {
		do {
			Assert(Result.Count < Result.Size);
			
			char *FileName = FindData.cFileName;
			if (FileName[0] == '.') continue;
			
			plore_file *File = Buffer + Result.Count;
			
			DWORD FlagsToIgnore = FILE_ATTRIBUTE_DEVICE    |
				                  FILE_ATTRIBUTE_ENCRYPTED |
				                  FILE_ATTRIBUTE_HIDDEN    |
				                  FILE_ATTRIBUTE_OFFLINE   |
				                  FILE_ATTRIBUTE_SYSTEM    |
				                  FILE_ATTRIBUTE_TEMPORARY;
			
			if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				File->Type = PloreFileNode_Directory;
			} else if (!(FindData.dwFileAttributes & FlagsToIgnore)) {
				File->Type = PloreFileNode_File;
			} else {
				Result.IgnoredCount++;
				continue;
			}
			
			DWORD BytesWritten = GetFullPathNameA(FindData.cFileName,
							                      ArrayCount(File->AbsolutePath),
							                      File->AbsolutePath,
							                      &File->FileNameInPath);
			
			Assert(BytesWritten);
			Result.Count++;
			
		} while (FindNextFile(FindHandle, &FindData));
	}
	
	Result.Succeeded = true;
	return(Result);
}

u8 FontBuffer[1<<21];
u8 TempBitmap[512*512];

internal plore_font 
FontInit(void)
{
	plore_font Result = {0};
	platform_readable_file FontFile = WindowsDebugOpenFile("data/fonts/Inconsolata-Light.ttf");
	platform_read_file_result TheFont = WindowsDebugReadEntireFile(FontFile, FontBuffer, ArrayCount(FontBuffer));
	Assert(TheFont.ReadSuccessfully);
	
	stbtt_BakeFontBitmap(FontBuffer, 0, 64.0, TempBitmap, 512, 512, 32, 96, Result.Data); // no guarantee this fits!
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

int WinMain (
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CmdLine,
    int ShowCommand)
{
#if PLORE_INTERNAL 
	DebugConsoleSetup();
#endif
    
	// TODO(Evan): GetModuleDirectory(?)
	char *PloreDLLPath = "build/plore.dll";
	char *TempDLLPath = "data/plore_temp.dll";
	char *LockPath = "build/lock.tmp";
	
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
		.ShowCursor = WindowsShowCursor,
		
		.GetDirectoryEntries = WindowsGetEntriesForDirectory,
		// @Hack
		#undef GetCurrentDirectory
		.GetCurrentDirectory = WindowsGetCurrentDirectory,
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
			WindowsDebugPrintLine("Unloading game code.");
			WindowsUnloadPloreCode(PloreCode);
			WindowsDebugPrintLine("Re-loading game code.");
			GlobalPloreInput.DLLWasReloaded = true;
		    PloreCode = WindowsLoadPloreCode(PloreDLLPath, TempDLLPath, LockPath);
		}
		
        if (PloreCode.DoOneFrame) {
			windows_timer FileCopyBeginTimer = WindowsGetTime();
			
			plore_render_list RenderList = PloreCode.DoOneFrame(&PloreMemory, &GlobalPloreInput, &WindowsPlatformAPI);
			
			windows_timer FileCopyEndTimer = WindowsGetTime();
			f64 FileCopyTimeInSeconds = ((f64) ((f64)FileCopyEndTimer.TicksNow - (f64)FileCopyBeginTimer.TicksNow) / (f64)FileCopyBeginTimer.Frequency);
			WindowsDebugPrintLine("File timing :: %f seconds", FileCopyTimeInSeconds);
			
			ImmediateBegin(WindowsContext.Width, WindowsContext.Height, MyFont);
			for (u64 I = 0; I < RenderList.QuadCount; I++) {
				DrawSquare(RenderList.Quads[I]);
			}
			
			f32 CursorY = WindowsContext.Height - GlobalPloreInput.ThisFrame.MouseY;
			WriteText(GlobalPloreInput.ThisFrame.MouseX, CursorY, "Hello.", RED_V4);
			SwapBuffers(WindowsContext.DeviceContext);
		}
		
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