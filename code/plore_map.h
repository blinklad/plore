//
// Dead simple, header-only, generic hash table.
// For implementation, define PLORE_MAP_IMPLEMENTATION in one file.
//
// Limitations and features:
// - Fixed-size backing store, key and value sized, initialized using a memory_arena: MapInit(Arena, struct key_type, struct value_type, Count)
// - Copies keys/values given to Insert: MapInsert(Map, &Key, &Value)
// - Zero-sized values are allowed to e.g., use a map as a set of key types.
// - Keys and values can be rvalues, but not string literals.
//
// - Iterable by "functional" style iterators: 
//   for (plore_map_iterator It = MapIter(Map); !It.Finished; It = MapIterNext(&Map, It)) { 
//       key_type *K = It.Key;
//
// - Open addressing and linear probing is used in conjunction with contiguous, data-oriented arrays for keys, values, and 'gravestones'.
//   This makes for an extremely straightforward implementation, and as a result it is trivially iterable, copyable and clearable.
//
// - Hash function is optimized for ~256 bit or larger key spaces.
//   In general, not a performance-oriented hash table by any stretch of the imagination.
//
// - Basic debug checks are provided in debug mode: 
//   The size of keys/values passed into Get/Insert/Remove are compared against what the map was initialized with. 
//   This does not work for a _lot_ of cases, but it better then nothing at all. 
//   A strongly typed approach is greatly preferred, however, the trade-offs of the current design are sufficient for plore's use cases.
//
#ifndef PLORE_MAP_H
#define PLORE_MAP_H

typedef struct plore_map plore_map;


//
// NOTE(Evan): Allow sizeof(void), which is obviously 0.
// Use case: MapInit(&SomeArena, key_type, void, Count), using the map as a set of key_type's.
//
// CLEANUP(Evan): Disabling errors like this is a bit rude.
//
#pragma warning(disable: 4034)


//
// Interface
//


//
// NOTE(Evan): 
// These macros are used for rudimentary *runtime* checking that pointer arguments can be of the same type, as using void * removes
// all type information.
// Obviously, checking pointer sizes does not work for different types of the same size, nor does it work at all in release builds.
// However, these macros are entirely transparent for the caller, except for when they want to insert non-lvalues,
// such as for using a map to test set membership.
//
// Every alternative to this has different tradeoffs in complexity, usability and performance, so let's just see how this goes.
//
#define MapInit(Arena, key_type, value_type, Count) _MapInit(Arena, sizeof(key_type), sizeof(value_type), Count)
#define MapInsert(Map, K, V)                        _MapInsert(Map, K, V, sizeof(*K), sizeof(*V))
#define SetInsert(Map, K, V)                        _MapInsert(Map, K, V, sizeof(*K), 0)
#define MapRemove(Map, K)                           _MapRemove(Map, K, sizeof(*K))
#define MapGet(Map, K)                              _MapGet(Map,    K, sizeof(*K))

typedef struct plore_map_get_result {
	void *Value;
	u64 Index;
	b64 Exists; // NOTE(Evan): Required for 0-sized values, i.e., testing set membership.
} plore_map_get_result;

typedef struct plore_map_remove_result {
	b64 KeyDidNotExist;
} plore_map_remove_result;

typedef struct plore_map_insert_result {
	b64 DidAlreadyExist;
	void *Key;
	void *Value;
} plore_map_insert_result;

typedef struct plore_map_iterator {
	b64 Finished;
	u64 _Index;
	void *Key;
	void *Value;
} plore_map_iterator;

internal plore_map_iterator
MapIter(plore_map *Map);

internal plore_map_iterator
MapIterNext(plore_map *Map, plore_map_iterator It);

// NOTE(Evan): Marks every key+value slot as unallocated without clearing.
internal void
MapReset(plore_map *Map);

// NOTE(Evan): Zeroes key+value memory.
internal void
MapClearMemory(plore_map *Map);

// NOTE(Evan): Returns destination.
internal plore_map *
MapCopy(plore_map *Source, plore_map *Destination);
	

// NOTE(Evan): If Lookup[Index].Allocated, then the contiguous arrays Keys[Index]/Values[Index] are valid.
typedef struct plore_map_key_lookup {
	b8 Allocated;
} plore_map_key_lookup;

typedef struct plore_map {
	u64 KeySize;
	u64 ValueSize;
	
	plore_map_key_lookup *Lookup;
	u64 Count;
	u64 Capacity;
	
	void *Keys;
	void *Values;
} plore_map;

//
// Internal functions
//


internal plore_map_get_result
_MapGet(plore_map *Map, void *Key, u64 DEBUGKeySize);

internal plore_map_remove_result
_MapRemove(plore_map *Map, void *Key, u64 DEBUGKeySize);

internal plore_map_insert_result 
_MapInsert(plore_map *Map, void *Key, void *Value, u64 DEBUGKeySize, u64 DEBUGValueSize);

internal plore_map
_MapInit(memory_arena *Arena, u64 KeySize, u64 DataSize, u64 Count);

#endif // PLORE_MAP_H

//
// Implementation
//

#ifdef PLORE_MAP_IMPLEMENTATION



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
#endif // PLORE_MAP_IMPLEMENTATION
