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
typedef uint32_t b32;
typedef uint64_t b64;

typedef float f32;
typedef double f64;



// NOTE(Evan): Helpful macros and meta-definitions
#define iRange(Variable, Count) \
for (i32 Variable = 0; Variable < Count; ++Variable)

#define uRange(Variable, Count) \
for (u64 Variable = 0; Variable < Count; ++Variable)

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
#define Assert(x) if (!(x)) { __debugbreak();}
#else
#define Assert(x) 
#endif


#define InvalidCodePath Assert(0)
// NOTE(Evan): Plore intrinsics
#define InvalidDefaultCase Assert(!"Invalid default case reached.");

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
ClearMemory(void *Memory, u64 Count) {
	u8 *Bytes = (u8 *)Memory;
	for (u64 Byte = 0; Byte < Count; Byte++) {
		*Bytes++ = 0;
	}
	
	return(Memory);
}

plore_inline void
MemoryCopy(void *Source, void *Destination, u64 Size) {
	u8 *S = (u8 *)Source;
	u8 *D = (u8 *)Destination;
	
	u64 Count = 0;
	while(Count++ < Size) {
		*D++ = *S++;
	}
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

#endif
