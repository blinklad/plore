#ifndef PLORE_GL_H
#define PLORE_GL_H

typedef struct plore_baked_font_data plore_baked_font_data;
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

typedef struct render_line {
	v2 P0;
	v2 P1;
	v4 Colour;
} render_line;

typedef enum quarter_circle_quadrant {
	QuarterCircleQuadrant_BottomRight,
	QuarterCircleQuadrant_TopRight,
	QuarterCircleQuadrant_TopLeft,
	QuarterCircleQuadrant_BottomLeft,
	_QuarterCircleQuadrant_ForceU64 = 0xffffffffull,
} quarter_circle_quadrant;

typedef struct render_quarter_circle {
	v2 P;
	f32 R;
	v4 Colour;
	quarter_circle_quadrant Quadrant;
} render_quarter_circle;

typedef struct render_text {
	rectangle Rect;
	char Text[256];
	v4 Colour;
	platform_texture_handle Texture;
	plore_baked_font_data *Data;
} render_text;

internal void
DrawSquare(render_quad Quad);

internal void
DrawCircle(render_circle Circle);

#endif //PLORE_GL_H
