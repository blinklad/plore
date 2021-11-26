#ifndef PLORE_GL_H
#define PLORE_GL_H

typedef struct render_circle {
	f32 R;
	v2 P;
	v4 Colour;
	b64 Outline;
} render_circle;

typedef struct render_quad {
	rectangle Rect;
	v4 Colour;
	b32 DrawOutline;
	platform_texture_handle Texture;
} render_quad;

typedef struct render_text {
	rectangle Rect;
	char Text[64];
	v4 Colour;
	b64 Centered;
	f32 Height;
} render_text;

internal void
DrawSquare(render_quad Quad);

internal void
DrawCircle(render_circle Circle);

#endif //PLORE_GL_H
