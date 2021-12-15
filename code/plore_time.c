#include "time.h" // localtime, strftime

internal char *
PloreTimeFormat(memory_arena *Arena, plore_time Time, char *Format) {
	u64 Size = 256; // @Hardcode
	char *Result = PushBytes(Arena, Size);
	
	struct tm *TimeInfo = localtime(&Time.T);
	strftime(Result, Size, Format, TimeInfo);
	
	return(Result);
}
