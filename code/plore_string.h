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
SubstringHelper(char *S, char *Substring, b64 CaseSensitive) {
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
			char aTest = *A++;
			char bTest = *B++;
			if (!CaseSensitive) {
				aTest = ToLower(aTest);
				bTest = ToLower(bTest);
			}
			
			if (aTest != bTest) break;
			if (J == SubLen-1) Result.IsContained = true;
		}
		
		if (Result.IsContained) break;
	}
	
	return(Result);
}

internal substring_result
SubstringNoCase(char *S, char *Substring) {
	substring_result Result = SubstringHelper(S, Substring, false); 
	
	return(Result);
}

internal substring_result
Substring(char *S, char *Substring) {
	substring_result Result = SubstringHelper(S, Substring, true);
	return(Result);
}

// NOTE(Evan): Returns whether A is lexicographically greater than A.
internal b64
StringCompare(char *A, char *B) {
	b64 Result = false;
	
	while (*A && *B) {
		if (*A != *B) {
			if (*B > *A) Result = false;
			else         Result = true;
			
			break;
		} else *A++, *B++;
	}
	
	return(Result);
}

internal b64
StringCompareReverse(char *A, char *B) {
	b64 Result = false;
	
	u64 LenA = StringLength(A);
	u64 LenB = StringLength(B);
	
	while (LenA-- && LenB--) {
		if (A[LenA] != B[LenB]) {
			if (B[LenB] > A[LenA]) Result = false;
			else                   Result = true;
			
			break;
		}
	}
	
	return(Result);
}

internal char *
StringConcatenate(char *Left, u64 LeftBufferSize, char *Right) {
	char *Result = Left;
	
	u64 Length = StringLength(Left);
	if (!LeftBufferSize)            return (Result);
	if (Length == LeftBufferSize-1) return(Result);
	
	Left += Length;
	u64 Count = LeftBufferSize -= Length;
	
	while (*Right) {
		*Left++ = *Right++;
		if (!Count--) break;
	}
	
	if (*Left) *Left = '\0';
	
	return(Result);
}

internal b64
StringsAreEqualIgnoreCase(char *A, char *B) {
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
StringsAreEqual(char *A, char *B) {
	while (*A && *B) {
		if (*A++ != *B++) return(false);
	}
	
	if ((!*A && *B) || (*A && !*B)) return(false);
	return(true);
}

plore_inline u64
StringCopy(char *Source, char *Destination, u64 BufferSize) {
	u64 BytesWritten = 0;
	Assert(Source && Destination && BufferSize);
	
	// NOTE(Evan): We return bytes written, *not* including the null terminator,
	// and currently allow empty strings.
	if (!*Source) {
		*Destination = '\0';
		return BytesWritten;
	}
	
	while (BytesWritten < BufferSize) {
		Destination[BytesWritten++] = *Source++;
		if (!*Source) {
			break;
		}
		
	}
	
	if (Destination == Source) Assert(0);
	// NOTE(Evan): Truncate the string.
	if (BytesWritten >= BufferSize) {
		Destination[BufferSize-1] = '\0';
	} else {
		Destination[BytesWritten] = '\0';
	}
	
	return(BytesWritten);
}
// credit: stb
#define ROTATE_LEFT(val, n)   (((val) << (n)) | ((val) >> (sizeof(u64)*8 - (n))))
#define ROTATE_RIGHT(val, n)  (((val) >> (n)) | ((val) << (sizeof(u64)*8 - (n))))

// TODO(Evan): Better hash function.
plore_inline u64 
HashString(char *String) {
	u64 Seed = 0x31415926;
	u64 Hash = Seed;
	while (*String) Hash = ROTATE_LEFT(Hash, 9) + (u8) *String++;
	
	// Thomas Wang 64-to-32 bit mix function, hopefully also works in 32 bits
	Hash ^= Seed;
	Hash = (~Hash) + (Hash << 18);
	Hash ^= Hash ^ ROTATE_RIGHT(Hash, 31);
	Hash = Hash * 21;
	Hash ^= Hash ^ ROTATE_RIGHT(Hash, 11);
	Hash += (Hash << 6);
	Hash ^= ROTATE_RIGHT(Hash, 22);
	return Hash+Seed;
}

plore_inline u64 
HashBytes(u8 *Bytes, u64 Count) {
	u64 Seed = 0x31415926;
	u64 Hash = Seed;
	while (Count--) Hash = ROTATE_LEFT(Hash, 9) + *Bytes++;
	
	// Thomas Wang 64-to-32 bit mix function, hopefully also works in 32 bits
	Hash ^= Seed;
	Hash = (~Hash) + (Hash << 18);
	Hash ^= Hash ^ ROTATE_RIGHT(Hash, 31);
	Hash = Hash * 21;
	Hash ^= Hash ^ ROTATE_RIGHT(Hash, 11);
	Hash += (Hash << 6);
	Hash ^= ROTATE_RIGHT(Hash, 22);
	return Hash+Seed;
}

#include "plore_memory.h"

typedef struct temp_string {
	memory_arena *Arena; // NOTE(Evan): Owning arena.
	char *Buffer;
	u64 Count;
	u64 Capacity;
} temp_string;

plore_inline temp_string
TempString(memory_arena *Arena, u64 Capacity) {
	temp_string Result = {
		.Arena = Arena,
		.Buffer = PushBytes(Arena, Capacity),
		.Capacity = Capacity,
	};
	
	return(Result);
}

#define TempCat(S, Format, ...)   S.Count += StringPrintSized(S.Buffer+S.Count, S.Capacity, Format, __VA_ARGS__)
#define TempPrint(S, Format, ...) S.Count = StringPrintSized(S.Buffer, S.Capacity, Format, __VA_ARGS__)


#endif //PLORE_STRING_H
