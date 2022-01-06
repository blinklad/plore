/* date = January 6th 2022 2:05 am */

#ifndef PLORE_MAP_H
#define PLORE_MAP_H

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

internal plore_map
MapInit(memory_arena *Arena, u64 KeySize, u64 DataSize, u64 Count);

internal void
MapClearMemory(plore_map *Map);

internal plore_map *
MapCopy(plore_map *Source, plore_map *Destination);
	
internal void
MapReset(plore_map *Map);

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
#define MapInsert(Map, K, V) _MapInsert(Map, K, V, sizeof(*K), sizeof(*V))
#define SetInsert(Map, K, V) _MapInsert(Map, K, V, sizeof(*K), 0)
#define MapRemove(Map, K)    _MapRemove(Map, K, sizeof(*K))
#define MapGet(Map, K)       _MapGet(Map,    K, sizeof(*K))

typedef struct plore_map_iterator {
	b64 Finished;
	u64 _Index;
	void *Key;
	void *Value;
} plore_map_iterator;

internal plore_map_iterator
MapIter(plore_map *Map);

internal plore_map_iterator
MapIterNext(plore_map *Map, plore_map_iterator *It);
	
#endif //PLORE_MAP_H
	