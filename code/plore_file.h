/* date = November 23rd 2021 0:12 pm */

#ifndef PLORE_FILE_H
#define PLORE_FILE_H

#include "plore_string.h"

typedef enum plore_file_node { 
	PloreFileNode_File, 
	PloreFileNode_Directory,
	_PloreFileNode_ForceU64 = 0xFFFFFFFF,
} plore_file_node;

// TODO(Evan): Separate extensions vs actual encoding. These are more like encoding hints.
// NOTE(Evan): Maybe we have a list of "decodable" file extensions, like PNG and .PDF, and assume anything else is UTF-8 encoded text.
// This depends entirely on how we want file open handlers vs file preview handlers to work!
// We could lean on the platform-specific file openers, but..
// * Linux doesn't have a great story with file opening (xdg-open is not particularly standard)
// * Windows has some exotic ideas for sane install paths

#define PLORE_FILE_EXTENSIONS    \
PLORE_X(Unknown, "", "unknown")  \
PLORE_X(BMP, ".bmp", "bitmap")   \
PLORE_X(PNG, ".png", "png")      \
PLORE_X(JPG, ".jpg", "jpeg")     \
PLORE_X(TXT, ".txt", "text")     \
PLORE_X(BAT, ".bat", "batch")  

typedef enum plore_file_extension {
#define PLORE_X(Name, _Ignored1, _Ignored2) PloreFileExtension_##Name##,
	PLORE_FILE_EXTENSIONS
#undef PLORE_X
	PloreFileExtension_Count,
	_PloreFileExtension_ForceU64 = 0xFFFFFFFF,
} plore_file_extension;


#define PLORE_X(Name, _Ignored, ShortString) ShortString
char *PloreFileExtensionShortStrings[] = {
	PLORE_FILE_EXTENSIONS,
};
#undef PLORE_X

#define PLORE_X(Name, LongString, _Ignored) LongString
char *PloreFileExtensionLongStrings[] = {
	PLORE_FILE_EXTENSIONS,
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
} plore_file;


typedef struct plore_get_file_extension_result {
	char *FilePart;
	char *ExtensionPart;
	b64 FoundOkay;
	plore_file_extension Extension;
} plore_get_file_extension_result;

internal plore_get_file_extension_result
GetFileExtension(char *FilePart) {
	plore_get_file_extension_result Result = {
		.FilePart = FilePart,
	};
	
	u64 NameSize = StringLength(FilePart);
	u64 ExtensionSize = NameSize;
	char *S = FilePart;
	while (*S) {
		if (*S == '.') break;
		else S++, NameSize--;
	}
	if (*S) {
		Result.ExtensionPart = S;
		#define PLORE_X(Name, E, _Ignored)                                  \
		if (!Result.FoundOkay && CStringsAreEqual(E, S)) {                  \
			Result.Extension = PloreFileExtension_##Name;                   \
			Result.FoundOkay = true;                                        \
		}
		
		PLORE_FILE_EXTENSIONS
		#undef PLORE_X
	}
	
	return(Result);
}

#endif //PLORE_FILE_H