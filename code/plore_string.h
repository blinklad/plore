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
IsUpper(char C) {
	b64 Result = (C >= 'A' && C <= 'Z');
	return(Result);
}


internal b64
IsLower(char C) {
	b64 Result = (C >= 'a' && C <= 'z');
	return(Result);
}


internal char
ToLower(char C) {
	char Result = C;
	if (IsUpper(C)) {
		Result += 32;
	}
	return(Result);
}

internal char
ToUpper(char C) {
	char Result = C;
	if (IsLower(C)) {
		Result -= 32;
	}
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

// @Cleanup, atoi (and more CRT) is not ideal.
internal i32
StringToI32(char *S) {
	i32 Result = atoi(S);
	return(Result);
}

typedef struct substring_result {
	u64 Index;
	b64 IsContained;
} substring_result;

internal substring_result
Substring(char *S, char *Substring) {
	substring_result Result = {0};
	
	if (!S || !Substring) return(Result);
	
	u64 SLen = StringLength(S);
	u64 SubLen = StringLength(Substring);
	if (SubLen > SLen) return(Result);
	
	for (u64 I = 0; I < SLen; I++) {
		if ((SLen - I) < SubLen) break;
		Result.Index = I;
		
		char *A = S+I;
		char *B = Substring;
		for (u64 J = 0; J < SubLen; J++) {
			if (*A++ != *B++) break;
			if (J == SubLen-1) Result.IsContained = true;
		}
		
		if (Result.IsContained) break;
	}
	
	return(Result);
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
