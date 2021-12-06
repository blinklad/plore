#ifndef PLORE_COMMON
#define PLORE_COMMON

// NOTE(Evan): Shared utilities, definitions and meta-macros!
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef uint32_t bool32;

typedef float float32;
typedef double float64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint32_t b8;
typedef uint32_t b32;
typedef uint64_t b64;

typedef float f32;
typedef double f64;


// NOTE(Evan): Some of these are intrinsics.

#define ArrayCount(Array) ((sizeof(Array) / sizeof(Array[0])))
#define Kilobytes(n) (((uint64) n)  * 1024ull)
#define Megabytes(n) (Kilobytes(n) * 1024ull)
#define Gigabytes(n) (Megabytes(n) * 1024ull)
#define IsPowerOfTwo(n) (((n) & (n - 1)) == 0)

#define SetFlag(Flags, Flag) ((Flags | Flag))

#define global static
#define local static
#define internal static
#define constant static const
#define plore_inline static inline

#define FlagsU64(X) (1ull << (X))
#define HasFlag(Flags, Flag) ((Flags & (Flag)))
#define ClearFlag(Flags, Bit) (Flags & (~Bit))

plore_inline b64
ToggleFlag(b64 Flags, b64 Flag) {
	b64 Result = Flags;
	if (HasFlag(Flags, Flag)) {
		Result = ClearFlag(Flags, Flag);
	} else {
		Result = SetFlag(Flags, Flag);
	}
	return(Result);
}

#if defined(PLORE_INTERNAL)
#define Assert(X) if (!(X)) { __debugbreak();}
#define StaticAssert(X) static u8 _StaticAssertArray[(X ? 1:-1)];
#else
#define Assert(X) 
#define StaticAssert(X)
#endif


#define InvalidCodePath Assert(0)
// NOTE(Evan): Plore intrinsics
#define InvalidDefaultCase default: Assert(!"Invalid default case reached.");

#if defined(PLORE_INTERNAL)
	#if defined(PLORE_WINDOWS)
	#define Debugger __debugbreak();
	#endif
#endif


typedef struct plore_range {
	void *Buffer;
	u64 Size;
} plore_range;

#define PloreRange(Buffer) ((plore_range) {&Buffer, ArrayCount(Buffer)})

plore_inline void *
MemoryClear(void *Memory, u64 ByteCount) {
	u8 *Bytes = (u8 *)Memory;
	for (u64 Byte = 0; Byte < ByteCount; Byte++) *Bytes++ = 0;
	
	return(Memory);
}

plore_inline void
MemoryCopy(void *Source, void *Destination, u64 ByteCount) {
	u8 *S = (u8 *)Source;
	u8 *D = (u8 *)Destination;
	
	u64 CopiedCount = 0;
	while(CopiedCount++ < ByteCount) *D++ = *S++;
}

plore_inline i64
MemoryCompare(void *_A, void *_B, u64 ByteCount) {
	u8 *A = (u8 *)_A;
	u8 *B = (u8 *)_B;
	
	i64 Result = 0;
	while(ByteCount--) {
		if (*A != *B) {
			Result = *A > *B ? +1 : -1;
			break;
		} else {
			A++, B++;
		}
	}
	
	return(Result);
}

plore_inline u64
CStringCopy(char *Source, char *Destination, u64 BufferSize) {
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


#define PloreSort(Items, Count, type)                          \
for (u64 I = 0; I < Count-1; I++) {                            \
	for (u64 J = 0; J < Count-I-1; J++) {                      \
		if (PloreSortPredicate(Items[J], Items[J+1])) {        \
			type Temp = Items[J];                              \
			Items[J] = Items[J+1];                             \
			Items[J+1] = Temp;                                 \
		}                                                      \
	}                                                          \
}                                                              \

#endif
