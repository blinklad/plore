/* date = January 6th 2022 2:05 am */

#ifndef PLORE_MAP_H
#define PLORE_MAP_H

typedef struct plore_map_key_lookup {
	b32 Allocated;
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
	