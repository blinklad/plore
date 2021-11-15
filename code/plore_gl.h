#ifndef PLORE_GL_H
#define PLORE_GL_H

typedef struct render_circle {
	f32 R;
	f32 Z;
	v2 P;
	v4 Colour;
	b64 Outline;
} render_circle;

typedef struct render_triangle {
	v2 P0, P1, P2;
	f32 Z;
	v4 Colour;
} render_triangle;


typedef struct render_quad {
	rectangle Rect;
	f32 Z;
	v4 Colour;
	b32 DrawOutline;
} render_quad;

typedef struct render_text {
	rectangle Rect;
	char Text[64];
	v4 Colour;
	b64 Centered;
} render_text;

internal void
DrawSquare(render_quad Quad);

internal void
DrawCircle(render_circle Circle);

internal void
DrawTriangle(render_triangle T);

#endif //PLORE_GL_H
