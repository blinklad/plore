plore_inline void *
_GetKey(plore_map *Map, u64 Index) {
	void *Result = (void *)((u8 *)Map->Keys + Index*Map->KeySize);
	
	return(Result);
}

plore_inline void *
_GetValue(plore_map *Map, u64 Index) {
	Assert(Map->ValueSize);
	void *Result = (void *)((u8 *)Map->Values + Index*Map->ValueSize);
	
	return(Result);
}

internal plore_map
_MapInit(memory_arena *Arena, u64 KeySize, u64 ValueSize, u64 Capacity) {
	Assert(Capacity);
	
	plore_map Result = {
		.KeySize   = KeySize,
		.ValueSize = ValueSize,
		.Lookup    = PushArray(Arena, plore_map_key_lookup, Capacity),
		.Keys      = PushBytes(Arena, Capacity*KeySize),
		.Values    = PushBytes(Arena, Capacity*ValueSize),
		.Capacity  = Capacity,
	};
	
	Assert(KeySize);
	Assert(Result.Lookup);
	Assert(Result.Keys);
	
	// NOTE(Evan): 0-sized values are legal for using maps as a set.
	if (ValueSize) Assert(Result.Values);
	
	return(Result);
}

internal plore_map_insert_result 
_MapInsert(plore_map *Map, void *Key, void *Value, u64 DEBUGKeySize, u64 DEBUGValueSize) {
	plore_map_insert_result Result = {0};
	Assert(Map);
	Assert(Map->Keys && Map->Lookup);
	Assert(Map->Count < Map->Capacity);
	
#if defined(PLORE_INTERNAL)
	Assert(DEBUGKeySize == Map->KeySize);
	Assert(DEBUGValueSize == Map->ValueSize);
#else
	(void)DEBUGKeySize;
	(void)DEBUGValueSize;
#endif
	
	u64 Index = HashBytes(Key, Map->KeySize) % Map->Capacity;
	plore_map_key_lookup *Lookup = Map->Lookup+Index;
	
	if (Lookup->Allocated && !BytesMatch(Key, _GetKey(Map, Index), Map->KeySize)) {
		for (;;) {
			Index = (Index + 1) % Map->Capacity;
			Lookup = Map->Lookup + Index;
			
			if (!Lookup->Allocated) {
				Lookup->Allocated = true;
				break;
			} else if (BytesMatch(Key, _GetKey(Map, Index), Map->KeySize)) {
				Result.DidAlreadyExist = true;
				break;
			}
		}
	}
	
	if (!Result.DidAlreadyExist) {
		Lookup->Allocated = true;
		Map->Count++;
		
		MemoryCopy(Key, _GetKey(Map, Index), Map->KeySize);
		if (Map->ValueSize) MemoryCopy(Value, _GetValue(Map, Index), Map->ValueSize);
	} 
	
	Result.Key = _GetKey(Map, Index);
	if (Map->ValueSize) Result.Value = _GetValue(Map, Index);
	
	return(Result);
}

internal plore_map_remove_result
_MapRemove(plore_map *Map, void *Key, u64 DEBUGKeySize) {
	plore_map_remove_result Result = {0};
	
#if defined(PLORE_INTERNAL)
	Assert(Map->KeySize == DEBUGKeySize);
#else
	(void)DEBUGKeySize;
#endif
	
	u64 Index = HashBytes(Key, Map->KeySize) % Map->Capacity;
	plore_map_key_lookup *Lookup = Map->Lookup+Index;
	u64 KeysChecked = 1;
	if (Lookup->Allocated && !BytesMatch(Key, _GetKey(Map, Index), Map->KeySize)) {
		for (;;) {
			Index = (Index + 1) % Map->Capacity;
			Lookup = Map->Lookup + Index;
			KeysChecked++;
			
			if (Lookup->Allocated && BytesMatch(Key, _GetKey(Map, Index), Map->KeySize)) {
				break;
			} 
			
			if (KeysChecked == Map->Capacity) {
				Result.KeyDidNotExist = true;
				break;
			}
		}
	}
	
	if (!Result.KeyDidNotExist) {
		Assert(Map->Count);
		Map->Count--;
		Map->Lookup[Index].Allocated = false;
		MemoryClear(_GetKey(Map, Index), Map->KeySize);
		if (Map->ValueSize) MemoryClear(_GetValue(Map, Index), Map->ValueSize);
	}
	
	return(Result);
}

typedef struct plore_map_get_result {
	void *Value;
	u64 Index;
	b64 Exists; // NOTE(Evan): Seems redundant, but required for 0-sized values, i.e., testing set membership.
} plore_map_get_result;

internal plore_map_get_result
_MapGet(plore_map *Map, void *Key, u64 DEBUGKeySize) {
	plore_map_get_result Result = {0};
	
#if defined(PLORE_INTERNAL)
	Assert(Map->KeySize == DEBUGKeySize);
#else
	(void)DEBUGKeySize;
#endif
	
	u64 Index = HashBytes(Key, Map->KeySize) % Map->Capacity;
	plore_map_key_lookup *Lookup = Map->Lookup+Index;
	
	u64 KeysChecked = 1;
	if (Lookup->Allocated) {
		if (!BytesMatch(Key, _GetKey(Map, Index), Map->KeySize)) {
			for (;;) {
				Index = (Index + 1) % Map->Capacity;
				Lookup = Map->Lookup+Index;
				KeysChecked++;
				
				if (!Lookup->Allocated) break;
				else if (BytesMatch(Key, _GetKey(Map, Index), Map->KeySize)) {
					Result.Index = Index;
					Result.Exists = true;
					if (Map->ValueSize) Result.Value = _GetValue(Map, Index);
					break;
				}
				
				if (KeysChecked == Map->Capacity) break;
			}
		} else {
			Result.Index = Index;
			Result.Exists = true;
			if (Map->ValueSize) Result.Value = _GetValue(Map, Index);
		}
	}
	
	return(Result);
}

internal void
MapReset(plore_map *Map) {
	MemoryClear(Map->Lookup, Map->Capacity*sizeof(plore_map_key_lookup));
	Map->Count = 0;
}

internal plore_map_iterator
MapIter(plore_map *Map) {
	plore_map_iterator Result = MapIterNext(Map, (plore_map_iterator){0});
	return(Result);
}

internal plore_map_iterator
MapIterNext(plore_map *Map, plore_map_iterator It) {
	plore_map_iterator Result = It;
	if (It._Index > Map->Capacity-1) {
		Result.Finished = true;
		return(Result);
	}
	
	for (u64 L = It._Index; L < Map->Capacity; L++) {
		if (Map->Lookup[L].Allocated) {
			Result._Index = L;
			Result.Key = _GetKey(Map, Result._Index);
			if (Map->ValueSize) Result.Value = _GetValue(Map, Result._Index);
			Result._Index++;
			
			return (Result);
		}
	}
	
	Result.Finished = true;
	return(Result);
}

internal void
MapClearMemory(plore_map *Map) {
	MemoryClear(Map->Lookup, Map->Capacity*sizeof(plore_map_key_lookup));
	MemoryClear(Map->Keys, Map->Capacity*Map->KeySize);
	MemoryClear(Map->Values, Map->Capacity*Map->ValueSize);
	Map->Count = 0;
}

internal plore_map *
MapCopy(plore_map *Source, plore_map *Destination) {
	Assert(Source && Destination);
	Assert(Source->Capacity == Destination->Capacity);
	Assert(Source->KeySize == Destination->KeySize);
	Assert(Source->ValueSize == Destination->ValueSize);
	
	MemoryCopy(Source->Lookup, Destination->Lookup, Source->Capacity*sizeof(plore_map_key_lookup));
	MemoryCopy(Source->Keys, Destination->Keys, Source->Capacity*Source->KeySize);
	MemoryCopy(Source->Values, Destination->Values, Source->Capacity*Source->ValueSize);
	Destination->Count = Source->Count;
	
	return(Destination);
}
