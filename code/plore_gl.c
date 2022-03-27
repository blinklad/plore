#include "plore_gl.h"
#include "plore_string.h"

global u64 GLWindowWidth;
global u64 GLWindowHeight;

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "generated/plore_baked_font.h"


//
// NOTE(Evan):
// Compatibility profile, immediate-mode GL interface.
//
// It goes without saying that this is woefully slow and all kinds of deprecated.
// But, those things are not a problem if I can develop 99% of plore in spite of them.
// Therefore, changing to a modern 4.x profile and using VAOs, shaders, etc is not a priority right now.
//

PLATFORM_CREATE_TEXTURE_HANDLE(GLCreateTextureHandle) {
	platform_texture_handle Result = {
		.Width = Desc.Width,
		.Height = Desc.Height,
	};

	glEnable(GL_TEXTURE_2D);

	GLenum GLProvidedPixelFormat = 0;
	GLenum GLTargetPixelFormat = 0;
	GLenum GLFilterMode = 0;
	switch (Desc.TargetPixelFormat) {
		case PixelFormat_RGBA8: {
			GLTargetPixelFormat = GL_RGBA8;
		} break;

		case PixelFormat_ALPHA: {
			GLTargetPixelFormat = GL_ALPHA;
		} break;

		case PixelFormat_BGRA8: {
			GLTargetPixelFormat = GL_BGRA;
		} break;

		InvalidDefaultCase;
	}
	switch (Desc.ProvidedPixelFormat) {

		case PixelFormat_RGBA8: {
			GLProvidedPixelFormat = GL_RGBA;
		} break;

		case PixelFormat_RGB8: {
			GLProvidedPixelFormat = GL_RGB;
		} break;

		case PixelFormat_ALPHA: {
			GLProvidedPixelFormat = GL_ALPHA;
		} break;

		case PixelFormat_BGRA8: {
			GLProvidedPixelFormat = GL_BGRA;
		} break;

		InvalidDefaultCase;
	}

	switch (Desc.FilterMode) {
		case FilterMode_Linear: {
			GLFilterMode = GL_LINEAR;
		} break;

		case FilterMode_Nearest: {
			GLFilterMode = GL_NEAREST;
		} break;

		InvalidDefaultCase;
	}

	GLuint Handle;
	glGenTextures(1, &Handle);
	Result.Opaque = Handle;
	glBindTexture(GL_TEXTURE_2D, Result.Opaque);

	glTexImage2D(GL_TEXTURE_2D, 0, GLTargetPixelFormat, Result.Width, Result.Height, 0, GLProvidedPixelFormat, GL_UNSIGNED_BYTE, Desc.Pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLFilterMode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLFilterMode);


	return(Result);
}


PLATFORM_DESTROY_TEXTURE_HANDLE(GLDestroyTextureHandle) {
	GLuint Opaque = (GLuint) Texture.Opaque;
	glDeleteTextures(1, &Opaque);
}

internal void
ImmediateBegin(u64 WindowWidth, u64 WindowHeight) {
	GLWindowWidth = WindowWidth;
	GLWindowHeight = WindowHeight;


	// NOTE(Evan): Reset the rendering state every frame.
	//             We shouldn't rely on OpenGL's cached state!
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glBindTexture(GL_TEXTURE_2D, 0);

		glEnable(GL_SCISSOR_TEST);
		glScissor(0, 0, GLWindowWidth, GLWindowHeight);

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_NOTEQUAL, 0);

		glClearColor(0.01f, 0.01f, 0.01f, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		glViewport(0, 0, GLWindowWidth, GLWindowHeight);
		glOrtho(0,
				GLWindowWidth,
				GLWindowHeight,
				0,
				0.0f,
				10.0f);
	}
}


internal void
SetBlendState() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

internal void
WriteText(plore_font *Font, render_text T) {
	stbtt_bakedchar *Data = 0;
	platform_texture_handle Handle = {0};

	Data = T.Data->Data;
	Handle = T.Texture;
	f32 X = T.Rect.P.X;
	f32 Y = T.Rect.P.Y;// - Font->Height;
	char *Text = T.Text;

	Assert(Handle.Opaque);
	//Y = GLWindowHeight - Y;

	// assume orthographic projection with units = screen pixels, origin at top left
	SetBlendState();
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, Handle.Opaque);
	glMatrixMode(GL_PROJECTION);
	glBegin(GL_QUADS);
		f32 StartX = X;
		f32 StartY = Y;
		char *StartText = Text;

		glColor4f(T.Colour.R, T.Colour.G, T.Colour.B, T.Colour.A);
		Text = StartText;
		X = StartX;
		Y = StartY;

		f32 MaxWidth = T.Rect.Span.W;
		f32 CurrentWidth = 0;
		f32 FontWidth = Data[0].xadvance;
		{
			while (*Text) {
				if (*Text >= 32 && *Text < 128) {
					stbtt_aligned_quad Q;
					stbtt_GetBakedQuad(Data, Handle.Width, Handle.Height, *Text-32, &X, &Y, &Q, 1);//1=opengl & d3d10+,0=d3d9

					glTexCoord2f(Q.s0, Q.t0); glVertex2f(Q.x0, Q.y0);
					glTexCoord2f(Q.s1, Q.t0); glVertex2f(Q.x1, Q.y0);
					glTexCoord2f(Q.s1, Q.t1); glVertex2f(Q.x1, Q.y1);
					glTexCoord2f(Q.s0, Q.t1); glVertex2f(Q.x0, Q.y1);

				} else if (*Text == '\t') {
					CurrentWidth += Data[0].xadvance*3;
					X += Data[0].xadvance*4;
				}
				++Text;
				CurrentWidth += Data[0].xadvance;
				if (CurrentWidth + 2 >= MaxWidth) break;
			}
		}
	glEnd();
}

internal void
DrawSquare(render_quad Quad) {
	v4 Colour = Quad.Colour;

	glMatrixMode(GL_PROJECTION);
	f32 W = Quad.Rect.Span.W;
	f32 H = Quad.Rect.Span.H;
	f32 X = Quad.Rect.P.X;
	f32 Y = Quad.Rect.P.Y;

	f32 Z = 1.0; // NOTE(Evan): Unused!

    f32 LeftX   = X;
    f32 RightX  = X + W;
    f32 BottomY = Y;
    f32 TopY    = Y + H;

	SetBlendState();
	if (Quad.Texture.Opaque) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, Quad.Texture.Opaque);
		glColor4f(1, 1, 1, 1);

		// @Hack, flip coordinate space of texture!
		f32 Temp = TopY;
		TopY = BottomY;
		BottomY = Temp;
	} else {
		glDisable(GL_TEXTURE_2D);
		glColor4f(Colour.R, Colour.G, Colour.B, Colour.A);
	}

    if (Quad.DrawOutline) {
        glBegin(GL_LINES);
		{
            glVertex2f(LeftX, BottomY);
            glVertex2f(RightX, BottomY);
            glVertex2f(LeftX, TopY);
            glVertex2f(RightX, TopY);
            glVertex2f(LeftX, BottomY);
            glVertex2f(LeftX, TopY);
            glVertex2f(RightX, BottomY);
            glVertex2f(RightX, TopY);
		}
        glEnd();
    } else {
        glBegin(GL_TRIANGLES);
		{
			glTexCoord2f(0, 0);
            glVertex2f(LeftX, BottomY);
			glTexCoord2f(1, 0);
            glVertex2f(RightX, BottomY);
			glTexCoord2f(1, 1);
            glVertex2f(RightX, TopY);

			glTexCoord2f(0, 0);
            glVertex2f(LeftX, BottomY);
			glTexCoord2f(1, 1);
            glVertex2f(RightX, TopY);
			glTexCoord2f(0, 1);
            glVertex2f(LeftX, TopY);
		}
        glEnd();
    }
}

internal void
DrawLine(render_line Line) {
	glMatrixMode(GL_PROJECTION);
	SetBlendState();
	glBindTexture(GL_TEXTURE_2D, 0);
	glColor4f(Line.Colour.R, Line.Colour.G, Line.Colour.B, Line.Colour.A);

	glBegin(GL_LINES);
		glVertex2f(Line.P0.X, Line.P0.Y);
		glVertex2f(Line.P1.X, Line.P1.Y);
	glEnd();
}

internal void
DrawQuarterCircle(render_quarter_circle Circle) {
	u32 Vertices = 64;
	f32 Theta = TwoPi32 / Vertices;
	f32 R = Circle.R;

	v2 C = Circle.P;
	u64 StartV = Circle.Quadrant*(Vertices/4);

	if (Circle.DrawOutline) {
		glBegin(GL_LINES);
		glColor4f(Circle.Colour.R, Circle.Colour.G, Circle.Colour.B, Circle.Colour.A);
		for (u64 V = StartV; V < StartV + Vertices/4; V++) {
			f32 P0X = C.X + R * Cos(V * Theta);
			f32 P0Y = C.Y + R * Sin(V * Theta);
			f32 P1X = C.X + R * Cos((V + 1) * Theta);
			f32 P1Y = C.Y + R * Sin((V + 1) * Theta);

			glVertex2f(P0X, P0Y);
			glVertex2f(P1X, P1Y);
		}
	} else {
		glBegin(GL_TRIANGLES);
		glColor4f(Circle.Colour.R, Circle.Colour.G, Circle.Colour.B, Circle.Colour.A);
		for (u64 V = StartV; V < StartV + Vertices/4; V++) {
			f32 P0X = C.X + R * Cos(V * Theta);
			f32 P0Y = C.Y + R * Sin(V * Theta);
			f32 P1X = C.X + R * Cos((V + 1) * Theta);
			f32 P1Y = C.Y + R * Sin((V + 1) * Theta);

			glVertex2f(C.X, C.Y);
			glVertex2f(P0X, P0Y);
			glVertex2f(P1X, P1Y);
		}
	}

	glEnd();
}

internal void
DrawCircle(render_circle Circle) {
	u32 Vertices = 32;
	f32 Theta = TwoPi32 / Vertices;
	f32 R = Circle.R;

	v2 C = Circle.P;
	for (u64 V = 0; V < Vertices; V++) {
		f32 P0X = C.X + R * Cos(V * Theta);
		f32 P0Y = C.Y + R * Sin(V * Theta);
	    f32 P1X = C.X + R * Cos((V + 1) * Theta);
		f32 P1Y = C.Y + R * Sin((V + 1) * Theta);

		v2 P0 = V2(P0X, P0Y);
		v2 P1 = V2(P1X, P1Y);
		DrawLine((render_line) {
					 .P0 = P0,
					 .P1 = P1,
					 .Colour = Circle.Colour,
				 });
	}
}


internal void
DoRender(plore_render_list *RenderList) {
	for (u64 I = 0; I < RenderList->CommandCount; I++) {
		render_command *C = RenderList->Commands + I;
		switch (C->Type) {
			case RenderCommandType_PrimitiveQuad: {
				DrawSquare(C->Quad);
			} break;

			case RenderCommandType_PrimitiveLine: {
				DrawLine(C->Line);
			} break;

			case RenderCommandType_PrimitiveQuarterCircle: {
				DrawQuarterCircle(C->QuarterCircle);
			} break;

			case RenderCommandType_Text: {
				WriteText(RenderList->Font, C->Text);
			} break;

			case RenderCommandType_Scissor: {
				rectangle R = C->Scissor.Rect;
				glScissor(R.P.X-0, R.P.Y-0, R.Span.W+0, R.Span.H+0);
			} break;

			InvalidDefaultCase;
		}
	}
}
