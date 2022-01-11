/* date = December 15th 2021 4:54 pm */

#ifndef PLORE_TIME_H
#define PLORE_TIME_H

#include <time.h> // time_t
#include "plore_memory.h"
typedef struct plore_time {
	time_t T;
} plore_time;

internal char *
PloreTimeFormat(memory_arena *Arena, plore_time Time, char *Format);

internal float64
PloreTimeDifferenceInSeconds(plore_time A, plore_time B);

internal plore_time
PloreTimeNow();

#endif //PLORE_TIME_H
