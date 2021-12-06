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
	
	_InteractState_ForceU64 = 0xFFFFFFFF,
} interact_state;

#define PLORE_KEYBOARD_AND_MOUSE \
PLORE_X(None,                                "none",          '\0')  \
PLORE_X(A,                                   "a",             'a')   \
PLORE_X(B,                                   "b",             'b')   \
PLORE_X(C,                                   "c",             'c')   \
PLORE_X(D,                                   "d",             'd')   \
PLORE_X(E,                                   "e",             'e')   \
PLORE_X(F,                                   "f",             'f')   \
PLORE_X(G,                                   "g",             'g')   \
PLORE_X(H,                                   "h",             'h')   \
PLORE_X(I,                                   "i",             'i')   \
PLORE_X(J,                                   "j",             'j')   \
PLORE_X(K,                                   "k",             'k')   \
PLORE_X(L,                                   "l",             'l')   \
PLORE_X(M,                                   "m",             'm')   \
PLORE_X(N,                                   "n",             'n')   \
PLORE_X(O,                                   "o",             'o')   \
PLORE_X(P,                                   "p",             'p')   \
PLORE_X(Q,                                   "q",             'q')   \
PLORE_X(R,                                   "r",             'r')   \
PLORE_X(S,                                   "s",             's')   \
PLORE_X(T,                                   "t",             't')   \
PLORE_X(U,                                   "u",             'u')   \
PLORE_X(V,                                   "v",             'v')   \
PLORE_X(W,                                   "w",             'w')   \
PLORE_X(X,                                   "x",             'x')   \
PLORE_X(Y,                                   "y",             'y')   \
PLORE_X(Z,                                   "z",             'z')   \
PLORE_X(Zero,                                "0",             '0')   \
PLORE_X(One,                                 "1",             '1')   \
PLORE_X(Two,                                 "2",             '2')   \
PLORE_X(Three,                               "3",             '3')   \
PLORE_X(Four,                                "4",             '4')   \
PLORE_X(Five,                                "5",             '5')   \
PLORE_X(Six,                                 "6",             '6')   \
PLORE_X(Seven,                               "7",             '7')   \
PLORE_X(Eight,                               "8",             '8')   \
PLORE_X(Nine,                                "9",             '9')   \
PLORE_X(Space,                               "<space>",       ' ')   \
PLORE_X(Return,                              "<ret>",         '\r')  \
PLORE_X(Colon,                               ":",             ':')   \
PLORE_X(SemiColon,                           ";",             ';')   \
PLORE_X(QuestionMark,                        "?",             '?')   \
PLORE_X(ExclamationMark,                     "!",             '!')   \
PLORE_X(At,                                  "@",             '@')   \
PLORE_X(Percent,                             "%",             '%')   \
PLORE_X(Dollar,                              "$",             '$')   \
PLORE_X(Caret,                               "^",             '^')   \
PLORE_X(Tilde,                               "~",             '~')   \
PLORE_X(Ampersand,                           "&",             '&')   \
PLORE_X(Asterisk,                            "*",             '*')   \
PLORE_X(ParenOpen,                           "(",             '(')   \
PLORE_X(ParenClosed,                         ")",             ')')   \
PLORE_X(BracketOpen,                         "[",             '[')   \
PLORE_X(BracketClosed,                       "]",             ']')   \
PLORE_X(BraceOpen,                           "{",             '{')   \
PLORE_X(BraceClosed,                         "}",             '}')   \
PLORE_X(Slash,                               "/",             '/')   \
PLORE_X(BackSlash,                           "\\",            '\\')  \
PLORE_X(Pipe,                                "|",             '|')   \
PLORE_X(Underscore,                          "_",             '_')   \
PLORE_X(Quote,                               "'",             '\'')  \
PLORE_X(DoubleQuote,                         "\"",            '"')   \
PLORE_X(RightArrow,                          ">",             '>')   \
PLORE_X(LeftArrow,                           "<",             '<')   \
PLORE_X(Period,                              ".",             '.')   \
PLORE_X(Comma,                               ",",             ',')   \
PLORE_X(Equals,                              "=",             '=')   \
PLORE_X(Backtick,                            "`",             '`')   \
PLORE_X(Plus,                                "+",             '+')   \
PLORE_X(Minus,                               "-",             '-')   \
PLORE_X(Pound,                               "#",             '#')   \
PLORE_X(PrintableCharacterCount,             "<debug>",       128)   \
PLORE_X(Ctrl,                                "<ctrl>",        129)   \
PLORE_X(Shift,                               "<shift>",       130)   \
PLORE_X(MouseLeft,                           "<mouse-left>",  131)   \
PLORE_X(MouseRight,                          "<mouse-right>", 132)   \
PLORE_X(Backspace,                           "<backspace>",   0x8)   \
PLORE_X(Tab,                                 "<tab>",         0x9)   

// MAINTENANCE(Evan): None of the symbolic strings should be larger then 32 bytes, including the null terminator.
#define PLORE_KEY_STRING_SIZE 32
#if 1
#define PLORE_X(_Ignored1, String, _Ignored2) \
StaticAssert(sizeof(String) < PLORE_KEY_STRING_SIZE, String " plore key string greater than allowed size");
PLORE_KEYBOARD_AND_MOUSE
#undef PLORE_X
#endif

#define PLORE_X(Name, _Ignored1, _Ignored2) \
PloreKey_##Name,
typedef enum plore_key {
	PLORE_KEYBOARD_AND_MOUSE
	PloreKey_Count,
	_PloreKey_ForceU64 = 0xFFFFFFFF,
} plore_key;
#undef PLORE_X

#define PLORE_X(Key, _Ignored1, Char) { PloreKey_##Key, Char },
typedef struct plore_key_lookup {
	plore_key K;
	char C;
} plore_key_lookup;
plore_key_lookup PloreKeyLookup[] = {
	PLORE_KEYBOARD_AND_MOUSE
};
#undef PLORE_X

#define PLORE_X(_Ignored1, String, _Ignored2) String,
char *PloreKeyStrings[] = {
	PLORE_KEYBOARD_AND_MOUSE
};
#undef PLORE_X

#define PLORE_X(_Ignored1, _Ignored2, Character) Character,
char PloreKeyCharacters[] = {
	PLORE_KEYBOARD_AND_MOUSE
};
#undef PLORE_X

typedef struct keyboard_and_mouse {
	b64 pKeys[PloreKey_Count]; // Presses this frame.
	b64 sKeys[PloreKey_Count]; // Shift down while this key was pressed.
	b64 bKeys[PloreKey_Count]; // Buffered presses this frame.
	b64 dKeys[PloreKey_Count]; // Down this frame.
	char TextInput[256];
	u64 TextInputCount;
	v2 MouseP;
    uint32 MouseWheel;
	b32 CursorIsShowing;
} keyboard_and_mouse;

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
	b64 Valid;
} plore_file_listing;

typedef struct plore_file_listing_cursor {
	plore_path Path;
	u64 Cursor;
} plore_file_listing_cursor;

typedef struct plore_file_listing_cursor_slot {
	b64 Allocated;
	plore_file_listing_cursor Cursor;
} plore_file_listing_cursor_slot;

typedef struct plore_file_context {
	plore_path Selected[32];
	u64 SelectedCount;
	
	plore_path Yanked[32];
	u64 YankedCount;
	
	plore_file_listing_cursor_slot *CursorSlots[512];
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