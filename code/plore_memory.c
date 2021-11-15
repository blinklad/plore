plore_inline memory_arena
CreateMemoryArena(void *Buffer, u64 BufferSizeInBytes) {
	memory_arena Result = {
		.Memory = Buffer,
		.Size = BufferSizeInBytes,
	};
	
	return(Result);
}

plore_inline u64
AlignedSize(u64 BytesUsed, u64 Alignment) {
    Assert(IsPowerOfTwo(Alignment));
    u64 Result = BytesUsed + ((BytesUsed + Alignment) & ~(-Alignment));
    
    return(Result);
}

plore_inline void
ClearArena(memory_arena *Arena) {
    Assert(Arena);
    Arena->BytesUsed = 0;
}

plore_inline memory_arena
SubArena(memory_arena *Arena, uint64 Size, u64 Alignment) {
    Assert(Arena);
    Assert(Size < (Arena->Size - Arena->BytesUsed));
    u64 EffectiveSize = AlignedSize(Arena->BytesUsed + Size, 16);
    Assert(EffectiveSize < Arena->Size);
    
    memory_arena Result = {
        .Memory = (u8 *) Arena->Memory + Arena->BytesUsed,
        .Size = Size,
    };
    
    Arena->BytesUsed += EffectiveSize;
    
    return(Result);
}

plore_inline void *
PushArena(memory_arena *Arena, uint64 BytesRequired, uint64 Alignment) {
    void *Result = 0;
    Assert(Arena && Arena->Memory);
    Assert(BytesRequired < (Arena->Size - Arena->BytesUsed));
    
    if (BytesRequired == 0) {
        return(Result);
    }
    
    u64 EffectiveSize = AlignedSize(BytesRequired, Alignment);
    Assert(BytesRequired + EffectiveSize <= Arena->Size);
    
    Result = (uint8 *)Arena->Memory + Arena->BytesUsed;
	MemoryClear(Result, EffectiveSize);
    Arena->BytesUsed += EffectiveSize;
    
    return(Result);
}

plore_inline temporary_memory
BeginTemporaryMemory(memory_arena *Arena) {
	Assert(Arena);
	temporary_memory Result = {
		.Arena = Arena,
		.BytesUsedOnCreation = Arena->BytesUsed,
	};
	
	return(Result);
}

plore_inline void
EndTemporaryMemory(temporary_memory *Memory) {
	Assert(Memory);
	Assert(Memory->Arena);
	Assert(Memory->Arena->BytesUsed >= Memory->BytesUsedOnCreation);
	Memory->Arena->BytesUsed = Memory->BytesUsedOnCreation;
}
