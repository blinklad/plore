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
	QuarterCircleQuadrant_BottomLeft,
	QuarterCircleQuadrant_TopLeft,
	QuarterCircleQuadrant_TopRight,
	_QuarterCircleQuadrant_ForceU64 = 0xffffffffull,
} quarter_circle_quadrant;

typedef struct render_quarter_circle {
	v2 P;
	f32 R;
	v4 Colour;
	quarter_circle_quadrant Quadrant;
	b64 DrawOutline;
} render_quarter_circle;

typedef struct render_text {
	rectangle Rect;
	char Text[256];
	v4 Colour;
	platform_texture_handle Texture;
	plore_baked_font_data *Data;
} render_text;

typedef struct render_scissor {
	rectangle Rect;
} render_scissor;

typedef enum render_command_type {
	RenderCommandType_PrimitiveQuad,
	RenderCommandType_PrimitiveCircle,
	RenderCommandType_PrimitiveQuarterCircle,
	RenderCommandType_PrimitiveLine,
	RenderCommandType_Text,
	RenderCommandType_Scissor,
	RenderCommandType_BindTexture,
	RenderCommandType_Count,
	_RenderCommandType_ForceU64 = 0xffffffffull,
} render_command_type;

typedef struct render_command {
	render_command_type Type;
	union {
		render_quad           Quad;
		render_circle         Circle;
		render_quarter_circle QuarterCircle;
		render_line           Line;
		render_text           Text;
		render_scissor        Scissor;
	};
} render_command;

internal void
DrawSquare(render_quad Quad);

internal void
DrawCircle(render_circle Circle);

#endif //PLORE_GL_H
