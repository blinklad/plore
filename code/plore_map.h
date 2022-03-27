//
// Dead simple, header-only, generic hash table.
// For implementation, define PLORE_MAP_IMPLEMENTATION in one file.
//
// Limitations and features:
// - Fixed-size backing store, key and value sized, initialized using a memory_arena.
//   You can specify if your keys are zero-terminated strings or binary blobs.
//       struct { uint64_t Key, Value; } *KV = MapInit(Arena, KV, false, Count);
//   Growing/shrinking the backing store is currently not supported, but possible.
//
// - Type-safe value insertion, using keys/values copied into MapInsert.
//   Using C90 designated initializer syntax (not required):
//   struct *KV Result = MapInsert(&Map, (&(struct kv) {
//                                        .K = 0xdeadbeef,
//                                        .V = { /* arbitrary bits */ }
//                                        }));
// - The extra parens around the second parameter are required, unfortunately, but you gain type-safe insertions this way.
//
// - Keys and values should be value-types, and although they can be rvalues, they cannot be string literals.
// - It's possible to change the interface to allow this, but it's a bit error prone.
//
// - Iterable using a type-safe macro:
//
//   /* Creates a local variable named `It`, with a compilation error if the map and specified type do not match. */
//   ForMap(Map, struct key_type) {
//       printf("{ %lld, %lld }\n", It->K, It->V);
//   }
//
//
// Performance notes:
// - In general, this is _not_ a performance-oriented hash table by any stretch of the imagination.
// - The benefit of an AoS key+value pair structure, however, is a type-safe and intuitive API.
// - Fixed-size hash tables tend to be small, which mitigates the cache misses from using array-of-structure key+value pairs.
// - Implemented using open addressing and linear probing over an allocation mask.
// - Hash function is optimized for ~256 bit or larger key spaces. Non-string keys are ideally at least 8 bytes to prevent too many collisions.
//
//
#ifndef PLORE_MAP_H
#define PLORE_MAP_H

typedef struct plore_map plore_map;

//
// Interface
//


#define MapInit(Arena, Lookup, KeyIsString, Capacity)       _MapInit(Arena, sizeof(*Lookup), sizeof(Lookup->K), sizeof(Lookup->V), (OffsetOfPtr((Lookup), K)), (OffsetOfPtr((Lookup), V)), KeyIsString, Capacity)
#define MapInsert(Lookup, KV)                               _MapInsert(Lookup, KV)
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

#define _MapInsert(Lookup, KV)    (_GetMapHeader(Lookup)->TempIndex = _MapGetInsertIndex(_GetMapHeader(Lookup), &KV->K), \
                                  Lookup[_GetMapHeader(Lookup)->TempIndex] = *KV,                                        \
                                  (Lookup + _GetMapHeader(Lookup)->TempIndex))

typedef struct plore_map_remove_result {
	b64 KeyDidNotExist;
} plore_map_remove_result;

typedef struct plore_map {
	b8 *Allocated;
	u64 TempIndex;
	u64 Count;
	u64 Capacity;
	u64 WrapperSize;
	u64 KeySize;
	u64 ValueSize;
	u64 KeyOffset;
	u64 ValueOffset;
	b64 KeyIsString;
} plore_map;

#endif // PLORE_MAP_H



//
// Implementation
//


//
// Internal macros
//
#define _GetMapHeader(Base)               ((plore_map *)(((u8 *)Base)   - sizeof(plore_map)))
#define _GetMapData(Header)               ((void *)     (((u8 *)Header) + sizeof(plore_map)))
#define _GetMapMetaSlot(Header, Index)    ((void *)     (((u8 *)_GetMapData(Header)) + Header->WrapperSize*(Index)))
#define _GetMapDefault(Header)            _GetMapMetaSlot(Header, _MapDefaultIndex(Map))
#define _GetMapLastRemoved(Header)        _GetMapMetaSlot(Header, _MapLastRemovedIndex(Map))
#define _MapDefaultIndex(Map)             (Map->Capacity+0)
#define _MapLastRemovedIndex(Map)         (Map->Capacity+1)

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

internal u64
_MapGetInsertIndex(plore_map *Map, void *Key) {
	Assert(Map);
	Assert(Map->Allocated);
	Assert(Map->Count < Map->Capacity);

	b64 DidAlreadyExist = false;
	u64 Index = _GetHashIndex(Map, Key);

	if (Map->Allocated[Index]) {
		if (!_KeysMatch(Map, Key, _GetKey(Map, Index))) {
			for (;;) {
				Index = (Index + 1) % Map->Capacity;

				if (!Map->Allocated[Index]) {
					break;
				} else if (_KeysMatch(Map, Key, _GetKey(Map, Index))) {
					DidAlreadyExist = true;
					break;
				}
			}
		} else {
			DidAlreadyExist = true;
		}
	}

	if (!DidAlreadyExist) {
		Map->Allocated[Index] = true;
		Map->Count++;
	}

	return(Index);
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
	void *DefaultPtr = (((u8 *)_GetMapData(Map)) + (_MapDefaultIndex(Map)*(Map->WrapperSize)));
	b64 Result =  DefaultPtr == KV;
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
