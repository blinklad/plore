#include "time.h" // localtime, strftime

internal char *
PloreTimeFormat(memory_arena *Arena, plore_time Time, char *Format) {
	u64 Size = 256; // @Hardcode
	char *Result = PushBytes(Arena, Size);
	
	struct tm *TimeInfo = localtime(&Time.T);
	strftime(Result, Size, Format, TimeInfo);
	
	return(Result);
}

internal float64
PloreTimeDifferenceInSeconds(plore_time A, plore_time B) {
	float64 Result = difftime(A.T, B.T);
	return(Result);
}

internal plore_time
PloreTimeNow() {
	plore_time Result = {
		.T = time(0),
	};
	
	return(Result);
}