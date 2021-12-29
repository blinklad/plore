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
#define StaticAssert(X, M) static_assert(X, M)
#else
#define Assert(X) 
#define StaticAssert(X, M)
#endif

#define ForArray(Count, Array) for (u64 Count = 0; Count < ArrayCount(Array); Count++)

#define InvalidCodePath Assert(0)
// NOTE(Evan): Plore intrinsics
#define InvalidDefaultCase default: Assert(!"Invalid default case reached.");

#if defined(PLORE_INTERNAL)
	#if defined(PLORE_WINDOWS)
	#define Debugger __debugbreak();
	#endif
#endif

#define ClearStruct(type) (type) {0}

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

#define StructCopy(Source, Destination, type)             MemoryCopy(Source, Destination, sizeof(type))
#define StructArrayCopy(Source, Destination, Count, type) MemoryCopy(Source, Destination, Count*sizeof(type))

#define StructArrayMatch(A, B, Count, type) (MemoryCompare(A, B, Count*sizeof(type)) == 0)
#define StructMatch(A, B, type)             (MemoryCompare(A, B, sizeof(type)) == 0)



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
