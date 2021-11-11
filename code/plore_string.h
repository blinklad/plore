#ifndef PLORE_STRING_H
#define PLORE_STRING_H

#ifndef STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_IMPLEMENTATION
#endif
#include "stb_sprintf.h"

#define StringPrint(Buffer, Format, ...) stbsp_sprintf(Buffer, Format, __VA_ARGS__)
#define StringPrintSized(Buffer, Size, Format, ...) stbsp_snprintf(Buffer, Size, Format, __VA_ARGS__)


internal u64
StringLength(char *S) {
	Assert(S);
	
	u64 Result = 0;
	while (*S++) Result++;
	
	return(Result);
}

typedef struct string_buffer {
	char *String;
	u64 Size;
} string_buffer;

#endif //PLORE_STRING_H
