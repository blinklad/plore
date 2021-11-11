#ifndef PLORE_MEMORY
#define PLORE_MEMORY

#include "plore_common.h"

typedef struct memory_arena {
    void *Memory;
    u64 Size;
    u64 BytesUsed;
} memory_arena;

typedef struct temporary_memory {
	memory_arena *Arena;
	u64 BytesUsedOnCreation;
} temporary_memory;

#define PushArray(Arena, Type, Count) PushArena(Arena, Count*sizeof(Type), 16)
#define PushStruct(Arena, Type) PushArena(Arena, sizeof(Type), 16)
#define PushBytes(Arena, Bytes) PushArena(Arena, Bytes, 16)

plore_inline memory_arena
CreateMemoryArena(void *Buffer, u64 BufferSizeInBytes);

plore_inline temporary_memory
BeginTemporaryMemory(memory_arena *Arena);

plore_inline void
EndTemporaryMemory(temporary_memory *Memory);

plore_inline u64
AlignedSize(u64 BytesUsed, u64 Alignment);

plore_inline void
ClearArena(memory_arena *Arena);

plore_inline memory_arena
SubArena(memory_arena *Arena, uint64 Size, u64 Alignment);

plore_inline void *
PushArena(memory_arena *Arena, uint64 BytesRequired, uint64 Alignment);

#endif