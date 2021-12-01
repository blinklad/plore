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

internal b64
IsAlpha(char C) {
	b64 Result = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z');
	return(Result);
}

internal b64
IsNumeric(char C) {
	b64 Result = (C >= '0' && C <= '9');
	return(Result);
}

internal char
AlphaToLower(char C) {
	if (!IsAlpha(C)) {
		return(C);
	}
	
	if (C >= 'A' && C <= 'Z') {
		C += 32;
	}
	
	return(C);
}


internal b64
CStringsAreEqualIgnoreCase(char *A, char *B) {
	while (*A && *B) {
		if (*A++ != *B++) {
			if (IsAlpha(*A) && IsAlpha(*B)) {
				if (AlphaToLower(*A) == AlphaToLower(*B)) continue;
			}
			return(false);
		}
	}
	
	if ((!*A && *B) || (*A && !*B)) return(false);
	return(true);
}

internal b64
CStringsAreEqual(char *A, char *B) {
	while (*A && *B) {
		if (*A++ != *B++) return(false);
	}
	
	if ((!*A && *B) || (*A && !*B)) return(false);
	return(true);
}

#endif //PLORE_STRING_H
