/* date = November 23rd 2021 0:12 pm */

#ifndef PLORE_FILE_H
#define PLORE_FILE_H

#include "plore_time.h"
#include "plore_string.h"

typedef enum plore_file_node { 
	PloreFileNode_File, 
	PloreFileNode_Directory,
	_PloreFileNode_ForceU64 = 0xFFFFFFFF,
} plore_file_node;

typedef struct plore_handler {
	char *Shell;
	char *Name;
} plore_handler;


#define PLORE_FILE_EXTENSION_HANDLER_MAX 8

#if defined(PLORE_WINDOWS)

#define PLORE_TERMINAL "\"C:/Program Files/Git/git-bash.exe\""
#define PLORE_EDITOR   "C:/tools/4coder/4ed.exe"
#define PLORE_HOME     "C:/Users/Evan"


#define PLORE_PHOTO_HANDLER            { .Name = "Windows Photo Viewer", .Shell = "rundll32.exe \"C:\\Program Files\\Windows Photo Viewer\\PhotoViewer.dll\", ImageView_Fullscreen" }
#define PLORE_BASIC_PHOTO_EDITOR       { .Name = "GIMP",                 .Shell = "\"C:\\Program Files\\GIMP 2\\bin\\gimp-2.10.exe\"" }
#define PLORE_ADVANCED_PHOTO_EDITOR    { .Name = "Paint",                .Shell = "\"C:\\Windows\\System32\\mspaint.exe\"" }
#define PLORE_TEXT_HANDLER             { .Name = "4Coder",               .Shell = PLORE_EDITOR  }


#elif defined(PLORE_LINUX)

#define PLORE_TERMINAL "st"
#define PLORE_EDITOR   "nvim"
#define PLORE_HOME     "/home/evan"

#define PLORE_PHOTO_HANDLER   { .Name = "Feh",    .Shell = "feh --fullscreen" }
#define PLORE_TEXT_HANDLER    { .Name = "neovim", .Shell = PLORE_EDITOR } 
#define PLORE_PHOTO_EDITOR    { .Name = "GIMP",   .Shell = "gimp" }


#else
#error Unsupported platform.
#endif


#define PLORE_FILE_EXTENSIONS                                                                                     \
PLORE_X(Unknown, "", "unknown", { { .Shell = "unknown"}                                                       } ) \
PLORE_X(BMP, "bmp", "bitmap",   { PLORE_PHOTO_HANDLER, PLORE_BASIC_PHOTO_EDITOR, PLORE_ADVANCED_PHOTO_EDITOR, } ) \
PLORE_X(PNG, "png", "png",      { PLORE_PHOTO_HANDLER, PLORE_BASIC_PHOTO_EDITOR, PLORE_ADVANCED_PHOTO_EDITOR, } ) \
PLORE_X(JPG, "jpg", "jpeg",     { PLORE_PHOTO_HANDLER, PLORE_BASIC_PHOTO_EDITOR, PLORE_ADVANCED_PHOTO_EDITOR, } ) \
PLORE_X(TXT, "txt", "text",     { PLORE_TEXT_HANDLER                                                          } ) \
PLORE_X(BAT, "bat", "batch",    { PLORE_TEXT_HANDLER                                                          } )  

typedef enum plore_file_extension {
#define PLORE_X(Name, _Ignored1, _Ignored2, ...) PloreFileExtension_##Name##,
	PLORE_FILE_EXTENSIONS
#undef PLORE_X
	PloreFileExtension_Count,
	_PloreFileExtension_ForceU64 = 0xFFFFFFFF,
} plore_file_extension;

#define PLORE_X(Name, _Ignored1, LongString, ...) LongString,
char *PloreFileExtensionLongStrings[] = {
	PLORE_FILE_EXTENSIONS
};
#undef PLORE_X

#define PLORE_X(Name, ShortString, _Ignored1, ...) ShortString,
char *PloreFileExtensionShortStrings[] = {
	PLORE_FILE_EXTENSIONS
};
#undef PLORE_X

#define PLORE_X(_Ignored1, _Ignored2, _Ignored3,  ...) __VA_ARGS__,
plore_handler PloreFileExtensionHandlers[][PLORE_FILE_EXTENSION_HANDLER_MAX] = {
	PLORE_FILE_EXTENSIONS
};
#undef PLORE_X

#define PLORE_MAX_PATH 256
typedef struct plore_path {
	char Absolute[PLORE_MAX_PATH];
	char FilePart[PLORE_MAX_PATH];
} plore_path;

typedef struct plore_file {
	plore_path Path;
	plore_file_node Type;
	plore_file_extension Extension;
	plore_time LastModification;
} plore_file;
#endif //PLORE_FILE_H

typedef struct plore_get_file_extension_result {
	char *FilePart;
	char *ExtensionPart;
	b64 FoundOkay;
	plore_file_extension Extension;
} plore_get_file_extension_result;

// NOTE(Evan): This grabs the *first* '.' starting from the left of the filename.
internal plore_get_file_extension_result
GetFileExtension(char *FilePart) {
	plore_get_file_extension_result Result = {
		.FilePart = FilePart,
	};
	
	u64 NameSize = StringLength(FilePart);
	u64 ExtensionSize = NameSize;
	char *S = FilePart;
	while (*S) {
		if (*S == '.') {
			S++; 
			break;
		}
		else {
			S++;
			NameSize--;
		}
	}
	if (*S) {
		Result.ExtensionPart = S;
		for (plore_file_extension E = 0; E < PloreFileExtension_Count; E++) {
			if (CStringsAreEqualIgnoreCase(PloreFileExtensionShortStrings[E], S)) {
				Result.Extension = E; 
				Result.FoundOkay = true;
			}
		}
	}
	
	return(Result);
}


// TODO(Evan): Generate this from metaprogram!
internal u64
GetHandlerCount(plore_file_extension Extension) {
	Assert(Extension < PloreFileExtension_Count);
	u64 H = 0;
	if (Extension) {
		for (H = 0; H < PLORE_FILE_EXTENSION_HANDLER_MAX; H++) if (!PloreFileExtensionHandlers[Extension][H].Shell) break;
	}
	
	return(H);
}