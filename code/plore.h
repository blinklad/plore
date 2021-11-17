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

// NOTE(Evan):
// Check for redundancy in X's keysym message pump when this is ported?
// We could probably map a symbolic name to a key.
#define PLORE_KEYBOARD_AND_MOUSE \
PLORE_X(W) \
PLORE_X(A) \
PLORE_X(S) \
PLORE_X(D) \
PLORE_X(Ctrl) \
PLORE_X(Shift)\
PLORE_X(Space) \
PLORE_X(Return) \
PLORE_X(MouseLeft) \
PLORE_X(MouseRight) \
PLORE_X(Plus) \
PLORE_X(Minus) \
PLORE_X(H) \
PLORE_X(J) \
PLORE_X(K) \
PLORE_X(L) \
PLORE_X(T) \
PLORE_X(Slash) \

#define PLORE_X(Name) b32 Name##IsPressed; b32 Name##IsDown; b32 Name##WasDown;
typedef struct keyboard_and_mouse {
	PLORE_KEYBOARD_AND_MOUSE
		
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
	plore_file_listing Directory;
} plore_file_listing_slot;

typedef struct plore_file_context {
	plore_file_listing *Current;
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