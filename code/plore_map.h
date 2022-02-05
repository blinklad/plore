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
#define MapInit(Arena, Lookup, KeyIsString, Capacity)       _MapInit(Arena, sizeof(*Lookup), sizeof(Lookup->K), sizeof(Lookup->V), (OffsetOfPtr((Lookup), K)), (OffsetOfPtr((Lookup), V)), KeyIsString, Capacity)
#define MapInsert(Lookup, K, V)                             _MapInsert(_GetMapHeader(Lookup), K, V)
#define MapInsertTest(Lookup, KV)                           _MapInsertTest(_GetMapHeader(Lookup), (KV))
#define MapRemove(Lookup, K)                                _MapRemove(_GetMapHeader(Lookup), K)
#define MapGet(Lookup, K)                                   (Lookup + _MapGet(_GetMapHeader(Lookup), K))
#define MapCount(Lookup)                                    _MapCount(_GetMapHeader(Lookup))
#define MapCapacity(Lookup)                                 _MapCapacity(_GetMapHeader(Lookup))
#define MapIsAllocated(Lookup, Index)                       _MapIsAllocated(_GetMapHeader(Lookup), Index)
#define MapReset(Lookup)                                    _MapReset(_GetMapHeader(Lookup))
#define MapFull(Lookup)                                     _MapFull(_GetMapHeader(Lookup))
#define MapExists(Lookup, K)                                (!MapIsDefault(Lookup, MapGet(Lookup, K)))
#define MapIsDefault(Lookup, KV)                            _MapIsDefault(_GetMapHeader(Lookup), KV)
#define MapDefaultIndex(Lookup)                             _MapDefaultIndex((_GetMapHeader(Lookup)))
#define MapNext(Lookup, KV)                                 _MapNext(_GetMapHeader(Lookup), KV)
#define MapIterBegin(Lookup)                                _MapIterBegin(_GetMapHeader(Lookup))
#define MapIterNext(Lookup, It)                             _MapIterNext(_GetMapHeader(Lookup), It)

#define ForMap(Map, type)                                        \
		for (type *_Index = MapIterBegin(Map), *It = 0;          \
			 It = Map + (uintptr)_Index, !MapIsDefault(Map, It); \
			 _Index = MapIterNext(Map, _Index))                  \

#define __MapInsert(Lookup, K, V)                                                                           \
                                                            _GetMapHeader(Lookup)->TempIndex,               \
                                                            Lookup[_GetMapHeader(Lookup)->TempIndex].K = K, \
                                                            Lookup[_GetMapHeader(Lookup)->TempIndex].V = V, \
                                                            Lookup + _GetMapHeader(Lookup)->TempIndex

internal void
_MapInsertTest(plore_map *Map, void *KV) {
}

typedef struct plore_map_remove_result {
	b64 KeyDidNotExist;
} plore_map_remove_result;

typedef struct plore_map_insert_result {
	b64 DidAlreadyExist;
	void *Key;
	void *Value;
} plore_map_insert_result;

// NOTE(Evan): Marks every key+value slot as unallocated without clearing.
internal void
_MapReset(plore_map *Map);

// NOTE(Evan): Zeroes key+value memory.
internal void
MapClearMemory(plore_map *Map);

typedef struct plore_map {
	b8 *Allocated;
	u64 Count;
	u64 Capacity;
	u64 WrapperSize;
	u64 KeySize;
	u64 ValueSize;
	u64 KeyOffset;
	u64 ValueOffset;
	b64 KeyIsString;
} plore_map;

#define _GetMapHeader(Base)    (plore_map *)(((u8 *)Base)   - sizeof(plore_map))
#define _GetMapData(Header)    (void *)     (((u8 *)Header) + sizeof(plore_map))
#define _GetMapDefault(Header) (void *)     (((u8 *)_GetMapData(Header)) + Header->WrapperSize*Header->Capacity)
#define _MapDefaultIndex(Map) (Map->Capacity)
//
// Internal functions
//


internal u64
_MapGet(plore_map *Map, void *Key);

internal plore_map_remove_result
_MapRemove(plore_map *Map, void *Key);

internal plore_map_insert_result
_MapInsert(plore_map *Map, void *Key, void *Value);

internal void *
_MapInit(memory_arena *Arena,
		u64 WrapperSize,
		u64 KeySize,
		u64 ValueSize,
		u64 KeyOffset,
		u64 ValueOffset,
		b64 KeyIsString,
		u64 Capacity);


#endif // PLORE_MAP_H

//
// Implementation
//

#ifdef PLORE_MAP_IMPLEMENTATION


plore_inline u64
_MapCount(plore_map *Map) {
	u64 Result = Map->Count;
	return(Result);
}

plore_inline u64
_MapCapacity(plore_map *Map) {
	u64 Result = Map->Capacity;
	return(Result);
}

plore_inline b64
_MapIsAllocated(plore_map *Map, u64 Index) {
	Assert(Map);
	if (Index < Map->Capacity) return (Map->Allocated[Index]);
	return(false);
}

plore_inline b64
_MapFull(plore_map *Map) {
	Assert(Map);
	b64 Result = Map->Count == Map->Capacity;
	return(Result);
}

plore_inline void *
_GetKey(plore_map *Map, u64 Index) {
	void *Result = (void *)((u8 *)_GetMapData(Map) + Index*Map->WrapperSize + Map->KeyOffset);

	return(Result);
}
plore_inline void *
_GetValue(plore_map *Map, u64 Index) {
	void *Result = (void *)((u8 *)_GetMapData(Map) + Index*Map->WrapperSize + Map->ValueOffset);

	return(Result);
}

internal void *
_MapInit(memory_arena *Arena, u64 WrapperSize, u64 KeySize, u64 ValueSize, u64 KeyOffset, u64 ValueOffset, b64 KeyIsString, u64 Capacity) {
	Assert(Capacity);
	Assert(KeySize);
	Assert(ValueSize);
	void *Result = PushBytes(Arena, WrapperSize*(Capacity+1) + sizeof(plore_map));

	plore_map *Header = Result;
	*Header = (plore_map) {
		.Allocated   = PushArray(Arena, b8, Capacity),
		.Capacity    = Capacity,
		.KeyIsString = KeyIsString,
		.WrapperSize = WrapperSize,
		.KeySize     = KeySize,
		.ValueSize   = ValueSize,
		.KeyOffset   = KeyOffset,
		.ValueOffset = ValueOffset,
	};
	Assert(Header->Allocated);

	Result = _GetMapData(Header);

	return(Result);
}

plore_inline void *
_MapIterBegin(plore_map *Map) {
	void *Result = (void *)((uintptr)_MapDefaultIndex(Map));
	if (Map->Count) {
		u64 Index = 0;
		while (!Map->Allocated[Index]) {
			Index += 1;
			if (Index == Map->Capacity) break;
		}

		Result = (void *)Index;
	}

	return(Result);
}

plore_inline void *
_MapIterNext(plore_map *Map, void *It) {
	void *Result = It;
	if (Map->Count) {
		uintptr Index = ((uintptr)Result)+1;
		if (Index < Map->Capacity) {
			while (!Map->Allocated[Index]) {
				Index += 1;
				if (Index == Map->Capacity) break;
			}

			Result = (void *)Index;
		} else {
			Result = (void *)(uintptr)_MapDefaultIndex(Map);
		}
	}

	return(Result);
}

plore_inline u64
_GetHashIndex(plore_map *Map, void *Key) {
	u64 Result = (Map->KeyIsString ? HashString(Key) : HashBytes(Key, Map->KeySize)) % Map->Capacity;
	return(Result);
}

plore_inline b64
_KeysMatch(plore_map *Map, void *K1, void *K2) {
	b64 Result = (Map->KeyIsString ? (StringsAreEqual(K1, K2)) : BytesMatch(K1, K2, Map->KeySize));
	return(Result);
}

internal plore_map_insert_result
_MapInsert(plore_map *Map, void *Key, void *Value) {
	Assert(Map);
	Assert(Map->Allocated);
	Assert(Map->Count < Map->Capacity);

	plore_map_insert_result Result = {0};

	u64 Index = _GetHashIndex(Map, Key);

	if (Map->Allocated[Index]) {
		if (!_KeysMatch(Map, Key, _GetKey(Map, Index))) {
			for (;;) {
				Index = (Index + 1) % Map->Capacity;

				if (!Map->Allocated[Index]) {
					break;
				} else if (_KeysMatch(Map, Key, _GetKey(Map, Index))) {
					Result.DidAlreadyExist = true;
					break;
				}
			}
		} else {
			Result.DidAlreadyExist = true;
		}
	}

	if (!Result.DidAlreadyExist) {
		Map->Allocated[Index] = true;
		Map->Count++;

		MemoryCopy(Key, _GetKey(Map, Index), Map->KeySize);
		MemoryCopy(Value, _GetValue(Map, Index), Map->ValueSize);
	}

	Result.Key = _GetKey(Map, Index);
	Result.Value = _GetValue(Map, Index);

	return(Result);
}

internal plore_map_remove_result
_MapRemove(plore_map *Map, void *Key) {
	plore_map_remove_result Result = {0};

	u64 Index = _GetHashIndex(Map, Key);
	u64 KeysChecked = 1;
	if (Map->Allocated[Index] && !_KeysMatch(Map, Key, _GetKey(Map, Index))) {
		for (;;) {
			Index = (Index + 1) % Map->Capacity;
			KeysChecked++;

			if (Map->Allocated[Index] && _KeysMatch(Map, Key, _GetKey(Map, Index))) {
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
		Map->Allocated[Index] = false;
		MemoryClear(_GetKey(Map, Index), Map->KeySize);
		MemoryClear(_GetValue(Map, Index), Map->ValueSize);
	}

	return(Result);
}

plore_inline b64
_MapIsDefault(plore_map *Map, void *KV) {
	b64 Result = (((u8 *)_GetMapData(Map)) + (_MapDefaultIndex(Map)*Map->WrapperSize)) == KV;
	return(Result);
}

internal u64
_MapGet(plore_map *Map, void *Key) {
	u64 Index = _GetHashIndex(Map, Key);
	u64 KeysChecked = 1;

	if (Map->Allocated[Index]) {
		if (!_KeysMatch(Map, Key, _GetKey(Map, Index))) {
			for (;;) {
				Index = (Index + 1) % Map->Capacity;
				KeysChecked++;

				if (!Map->Allocated[Index]) {
					Index = _MapDefaultIndex(Map);
					break;
				} else if (_KeysMatch(Map, Key, _GetKey(Map, Index))) {
					break;
				}

				if (KeysChecked == Map->Capacity) {
					Index = _MapDefaultIndex(Map);
					break;
				}
			}
		}
	} else {
		Index = _MapDefaultIndex(Map);
	}

	return(Index);
}

internal void
_MapReset(plore_map *Map) {
	MemoryClear(Map->Allocated, Map->Capacity*sizeof(b8));
	Map->Count = 0;
}

internal void
MapClearMemory(plore_map *Map) {
	MemoryClear(Map->Allocated, Map->Capacity*sizeof(b8));
	MemoryClear(_GetMapData(Map), Map->Capacity*Map->WrapperSize);
	Map->Count = 0;
}

#endif // PLORE_MAP_IMPLEMENTATION
