#ifndef PLORE_H
#define PLORE_H

#include "plore_common.h"
#include "plore_math.h"
#include "plore_memory.h"
#include "plore_platform.h"
#include "plore_gl.h"


#if defined(PLORE_WINDOWS)
#define PLORE_EXPORT __declspec(dllexport)
#elif defined(PLORE_LINUX)
#define PLORE_EXPORT __attribute__ ((dllexport))
#else 
#define PLORE_EXPORT 
#endif

struct plore_state;
typedef struct plore_state plore_state;

// HACK(Evan): Currently, we bake a "display" and "debug" font!
#include "stb_truetype.h"
typedef struct plore_font {
	struct {
		stbtt_bakedchar Data[96]; // ASCII 32..126 is 95 glyphs
		platform_texture_handle Handle;
		f32 Height;
	} Fonts[2];
} plore_font;


typedef enum interact_state {
	InteractState_FileExplorer,
	InteractState_CommandHistory,
	
	_InteractState_ForceU64 = 0xFFFFFFFF,
} interact_state;

#define PLORE_KEYBOARD_AND_MOUSE \
PLORE_X(A,                "a")                  \
PLORE_X(B,                "b")                  \
PLORE_X(C,                "c")                  \
PLORE_X(D,                "d")                  \
PLORE_X(E,                "e")                  \
PLORE_X(F,                "f")                  \
PLORE_X(G,                "g")                  \
PLORE_X(H,                "h")                  \
PLORE_X(I,                "i")                  \
PLORE_X(J,                "j")                  \
PLORE_X(K,                "k")                  \
PLORE_X(L,                "l")                  \
PLORE_X(M,                "m")                  \
PLORE_X(N,                "n")                  \
PLORE_X(O,                "o")                  \
PLORE_X(P,                "p")                  \
PLORE_X(Q,                "q")                  \
PLORE_X(R,                "r")                  \
PLORE_X(S,                "s")                  \
PLORE_X(T,                "t")                  \
PLORE_X(U,                "u")                  \
PLORE_X(V,                "v")                  \
PLORE_X(W,                "w")                  \
PLORE_X(X,                "x")                  \
PLORE_X(Y,                "y")                  \
PLORE_X(Z,                "z")                  \
PLORE_X(Zero,             "0")                  \
PLORE_X(One,              "1")                  \
PLORE_X(Two,              "2")                  \
PLORE_X(Three,            "3")                  \
PLORE_X(Four,             "4")                  \
PLORE_X(Five,             "5")                  \
PLORE_X(Six,              "6")                  \
PLORE_X(Seven,            "7")                  \
PLORE_X(Eight,            "8")                  \
PLORE_X(Nine,             "9")                  \
PLORE_X(Plus,             "+")                  \
PLORE_X(Minus,            "-")                  \
PLORE_X(Slash,            "/")                  \
PLORE_X(Space,            "<space>")            \
PLORE_X(Return,           "<ret>")              \
PLORE_X(Ctrl,             "<ctrl>")             \
PLORE_X(Shift,            "<shift>")            \
PLORE_X(MouseLeft,        "<m-l>")              \
PLORE_X(MouseRight,       "<m-r>")   
// MAINTENANCE(Evan): None of the symbolic strings should be larger then 8 bytes, including the null terminator.

#define PLORE_X(_Ignored, String) \
String,
char *PloreKeyStrings[] = {
	PLORE_KEYBOARD_AND_MOUSE
};
#undef PLORE_X

#define PLORE_X(Name, _Ignored) \
PloreKey_##Name,
typedef enum plore_key {
	PLORE_KEYBOARD_AND_MOUSE
	PloreKey_Count,
	_PloreKey_ForceU64 = 0xFFFFFFFF,
} plore_key;
#undef PLORE_X

#define PLORE_X(Name, _Ignored) \
	b32 Name##IsPressed;  \
	b32 Name##IsDown;     \
	b32 Name##WasDown;
typedef struct keyboard_and_mouse {
	PLORE_KEYBOARD_AND_MOUSE
		
	b64 pKeys[PloreKey_Count]; // Presses this frame.
	b64 bKeys[PloreKey_Count]; // Buffered presses this frame.
	v2 MouseP;
    uint32 MouseWheel;
	b32 CursorIsShowing;
} keyboard_and_mouse;
#undef PLORE_X

typedef struct plore_input {
	keyboard_and_mouse LastFrame;
	keyboard_and_mouse ThisFrame;
	f32 Time;
	f32 DT;
	b32 DLLWasReloaded;
} plore_input;

typedef struct plore_memory {
    memory_arena PermanentStorage;
} plore_memory;

typedef struct plore_file_listing {
	plore_file File;
	plore_file Entries[256]; // NOTE(Evan): Directories only.
	
	u64 Count;
	u64 Cursor;
} plore_file_listing;

typedef struct plore_file_listing_slot {
	b64 Allocated;
	plore_file_listing Listing;
} plore_file_listing_slot;

typedef struct plore_file_context {
	plore_file_listing *Current;
	
	plore_file_listing *Selected[32];
	u64 SelectedCount;
	
	plore_file_listing *Yanked[32];
	u64 YankedCount;
	
	plore_file_listing_slot *FileSlots[512];
	u64 FileCount;
	
	b64 InTopLevelDirectory;
} plore_file_context;

typedef struct plore_render_list {
	render_quad Quads[512];
	u64 QuadCount;
	
	render_text Text[512];
	u64 TextCount;
	
	plore_font *Font;
} plore_render_list;

#define PLORE_DO_ONE_FRAME(name) PLORE_EXPORT plore_render_list name(plore_memory *PloreMemory, plore_input *PloreInput, platform_api *PlatformAPI)
typedef PLORE_DO_ONE_FRAME(plore_do_one_frame);


// TODO(Evan): Development constants file
enum { 
    DEFAULT_WINDOW_WIDTH = 1920,
    DEFAULT_WINDOW_HEIGHT = 1080,
};


#endif