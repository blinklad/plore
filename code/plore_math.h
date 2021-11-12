#ifndef PLORE_MATH_H
#define PLORE_MATH_H

#include <math.h>
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#define HMM_PREFIX 
#include "HandmadeMath.h"

typedef union hmm_vec2 v2;
typedef union hmm_vec3 v3;
typedef union hmm_vec4 v4;

typedef union hmm_mat4 mat4;

constant float32 Pi32     = 3.141592653589793238462643f;
constant float32 TwoPi32  = 2.0f * 3.141592653589793238462643f;
constant float32 HalfPi32 = 3.141592653589793238462643f / 2.0f;
constant float32 QuarterPi32 = 3.141592653589793238462643f / 4.0f;


constant v3 RED_V3 =   { 1, 0, 0 };
constant v3 GREEN_V3 = { 0, 1, 0 };
constant v3 BLUE_V3 =  { 0, 0, 1 };

constant v4 RED_V4      = { 1, 0, 0, 1};
constant v4 GREEN_V4    = { 0, 1, 0, 1};
constant v4 BLUE_V4     = { 0, 0, 1, 1};
constant v4 YELLOW_V4   = { 1, 1, 0, 1};
constant v4 MAGENTA_V4  = { 1, 0, 1, 1};
constant v4 WHITE_V4    = { 1, 1, 1, 1};

#define Sin(x) sinf(x)
#define Cos(x) cosf(x)

typedef struct rectangle {
	v2 Centre;
	v2 HalfSpan;
} rectangle;

typedef rectangle rect;

typedef struct aabb {
	v2 P;
	v2 R;
} aabb;

typedef struct circle {
	v2 P;
	f32 R;
} circle;

typedef struct triangle {
	v2 A;
	v2 B;
	v2 C;
} triangle;

plore_inline b32
PointInCircle(v2 P, circle Circle) {
	v2 D = SubtractVec2(P, Circle.P);
	f32 Inner = DotVec2(D, D);
	b32 Result = Inner < Circle.R * Circle.R;
	return(Result);
}

plore_inline aabb
AABBFromPointHalfSpan(v2 P, v2 HalfSpan) {
	aabb Result = {
		.P = P,
		.R = HalfSpan,
	};
	
	return(Result);
}

plore_inline aabb
AABBFromPointSpan(v2 P, v2 Span) {
	aabb Result = {
		.P = P,
		.R = { .X = Span.X / 2.0f, .Y = Span.Y / 2.0f },
	};
	
	return(Result);
}

plore_inline v2
ClosestPointAABB(v2 P, aabb A) {
	v2 Result = {
		.X = Clamp(P.X, A.P.X - A.R.X, A.P.X + A.R.X),
		.Y = Clamp(P.Y, A.P.Y - A.R.Y, A.P.Y + A.R.Y),
	};
	
	return(Result);
}

plore_inline b32
AABBOverlapsAABB(aabb A, aabb B) {
	if (Abs(A.P.X - B.P.X) >= A.R.X + B.R.X) return(false);
	if (Abs(A.P.Y - B.P.Y) >= A.R.Y + B.R.Y) return(false);
	
	return(true);
}

// NOTE(Evan): Out params must start zeroed 
internal b32
TestAABBAABB(aabb A, aabb B, v2 rV, v2 *nOut, f32 *tStartOut, f32 *tEndOut) {
	f32 MaxA[] = { A.P.X + A.R.X, A.P.Y + A.R.Y };
	f32 MaxB[] = { B.P.X + B.R.X, B.P.Y + B.R.Y };
	
	f32 MinA[] = { A.P.X - A.R.X, A.P.Y - A.R.Y };
	f32 MinB[] = { B.P.X - B.R.X, B.P.Y - B.R.Y };
	
	f32 EPSILON = 0.001f;
	
	f32 tStart = 0.0f;
	f32 tEnd = 1.0f;
	uRange(Axis, 2) {
		if (rV.E[Axis] < 0.0f) {
			if (MaxB[Axis] < MinA[Axis]) return(false);
			if (MaxA[Axis] < MinB[Axis]) {
				tStart = Max((MaxA[Axis] - MinB[Axis]) / rV.E[Axis], tStart);
			}
			if (MaxB[Axis] > MinA[Axis]) {
				tEnd = Min((MinA[Axis] - MaxB[Axis]) / rV.E[Axis], tEnd);
			}
		}
		
		if (rV.E[Axis] > 0.0f) {
			if (MinB[Axis] > MaxA[Axis]) return(false);
			if (MaxB[Axis] < MinA[Axis]) {
				tStart = Max((MinA[Axis] - MaxB[Axis]) / rV.E[Axis], tStart);
			}
			if (MaxA[Axis] > MinB[Axis]) {
				tEnd = Min((MaxA[Axis] - MinB[Axis]) / rV.E[Axis], tEnd);
			}
		}
		
		// NOTE(Evan): If there's no movement on this axis, and there exists a separating axis...
		//             Well, they can't overlap by definition, so terminate.
		if (rV.E[Axis] == 0.0f) {
			if (A.R.E[Axis] + B.R.E[Axis] < Abs(A.P.E[Axis] - B.P.E[Axis])) return(false);
		}
		
		if (tStart > tEnd) return(false);
		
	}
	
	
	v2 D = SubtractVec2(A.P, B.P);
	if ((A.R.X + B.R.X - Abs(D.X)) < (A.R.Y + B.R.Y - Abs(D.Y))) {
		if (D.X < 0) {
			nOut->X = -1;
		} else {
			nOut->X = +1;
		}
	} else {
		if (D.Y > 0) {
			nOut->Y = +1;
		} else {
			nOut->Y = -1;
		}
	}
	
	*tStartOut = Max(tStart - EPSILON, EPSILON);
	*tEndOut = tEnd;
	return(true);
}


plore_inline rectangle
RectangleHalfSpan(v2 Centre, v2 HalfSpan) {

    rectangle Result = {
		.Centre = Centre, .HalfSpan = HalfSpan,
    };
    
    return(Result);
}


plore_inline rectangle
RectangleBottomLeftSpan(v2 P, v2 Span) {
	P.X += Span.W/2.0f;
	P.Y += Span.H/2.0f;
	Span.W /= 2.0f;
	Span.H /= 2.0f;
	
	rectangle Result = {
		.Centre = P,
		.HalfSpan = Span,
	};
	
	return(Result);
}

plore_inline rectangle
RectangleSpan(v2 Centre, v2 Span) {
    rectangle Result = RectangleHalfSpan(Centre, Span);
    
    return(Result);
}

plore_inline v2
RectangleToHalfSpan(rectangle Rect){
	v2 Result = Rect.HalfSpan;
	
	return(Result);
}

plore_inline v2
RectangleTopLeft(rectangle Rect) {
	v2 Result = {0};
	
	Result.X = Rect.Centre.X - Rect.HalfSpan.W;
	Result.Y = Rect.Centre.Y - Rect.HalfSpan.H;
	
	return(Result);
}


plore_inline v2
RectangleBottomCentre(rectangle Rect) {
	v2 Result = {0};
	
	Result.X = Rect.Centre.X;
	Result.Y = Rect.Centre.Y - Rect.HalfSpan.H;
	
	return(Result);
}

plore_inline v2
RectangleTopCentre(rectangle Rect) {
	v2 Result = {0};
	
	Result.X = Rect.Centre.X;
	Result.Y = Rect.Centre.Y + Rect.HalfSpan.H;
	
	return(Result);
}

plore_inline u32
CeilingToU32(u32 A, u32 B) {
	u32 Result = 0;
	Result = (u32)Ceiling((f32)A / (f32)B);
	return(Result);
}

plore_inline v2
RectangleGetBottomLeft(rectangle Rect) {
	v2 Result = SubtractVec2(Rect.Centre, Rect.HalfSpan);
	
	return(Result);
}

plore_inline v2
RectangleGetUpperRight(rectangle Rect) {
	v2 Result = AddVec2(Rect.Centre, Rect.HalfSpan);
	
	return(Result);
}

b32
IsWithinRectangleInclusive(v2 P, rectangle Rect) {
	v2 BottomLeft = RectangleGetBottomLeft(Rect);
	v2 UpperRight = RectangleGetUpperRight(Rect);
	b32 Result = (P.X >= BottomLeft.X &&
				  P.X <= UpperRight.X &&
				  P.Y >= BottomLeft.Y &&
				  P.Y <= UpperRight.Y);
	
	return(Result);
}

triangle 
Triangle(v2 A, v2 B, v2 C) {
	triangle Result = { A, B, C };
	
	return(Result);
}

	
#endif