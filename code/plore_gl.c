#include "plore_gl.h"
#include "plore_string.h"

global u64 GLWindowWidth;
global u64 GLWindowHeight;

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

internal void 
WriteText(plore_font *Font, render_text T) {
	stbtt_bakedchar *Data = 0;
	platform_texture_handle Handle = {0};
	
	// HACK(Evan): Font handling!
	u64 Index = 0;
	if (T.Height == 32.0f) {
		Index = 0;
	} else {
		Index = 1;
	}
	
	Data = Font->Fonts[Index].Data;
	Handle = Font->Fonts[Index].Handle;
	
	
	if (T.Centered) {
		T.Rect.P.X -= StringLength(T.Text)/2.0f * Data[0].xadvance;
	}
	
	f32 X = T.Rect.P.X;
	f32 Y = T.Rect.P.Y;// - Font->Height;
	char *Text = T.Text;
	
	Assert(Handle.Opaque);
	//Y = GLWindowHeight - Y;
	
	// assume orthographic projection with units = screen pixels, origin at top left
	glMatrixMode(GL_PROJECTION);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, Handle.Opaque);
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
			}
			++Text;
			CurrentWidth += Data[0].xadvance;
			if (CurrentWidth + 2 >= MaxWidth) break; 
		}
	}
	glEnd();
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
		
		glDisable(GL_DEPTH_TEST);
		glClear(GL_DEPTH_BUFFER_BIT);
		glDepthFunc(GL_LESS);
		
		glEnable(GL_ALPHA_TEST);
//		glAlphaFunc(GL_NOTEQUAL, 0);
		
		glClearColor(0.01f, 0.01f, 0.01f, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		
		glViewport(0, 0, GLWindowWidth, GLWindowHeight);
		glOrtho(0,
				GLWindowWidth,
				GLWindowHeight,
				0,
				0.00f,
				-10.0f);
	}
}

internal void
DrawSquare(render_quad Quad) {
	v4 Colour = Quad.Colour;
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	f32 W = Quad.Rect.Span.W;
	f32 H = Quad.Rect.Span.H;
	f32 X = Quad.Rect.P.X;
	f32 Y = Quad.Rect.P.Y;
	f32 Z = Quad.Z;
	
    f32 LeftX   = X;
    f32 RightX  = X + W;
    f32 BottomY = Y;
    f32 TopY    = Y + H;
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	Colour.RGB = MultiplyVec3f(Colour.RGB, Colour.A);
    glColor4f(Colour.R, Colour.G, Colour.B, Colour.A);
	
    if (Quad.DrawOutline) {
        glBegin(GL_LINES); 
		{
            glVertex3f(LeftX, BottomY, Z);
            glVertex3f(RightX, BottomY, Z);
            glVertex3f(LeftX, TopY, Z);
            glVertex3f(RightX, TopY, Z);
            glVertex3f(LeftX, BottomY, Z);
            glVertex3f(LeftX, TopY, Z);
            glVertex3f(RightX, BottomY, Z);
            glVertex3f(RightX, TopY, Z);
		}
        glEnd();
    } else {
        glBegin(GL_TRIANGLES);
		{
			glTexCoord2f(0, 0);
            glVertex3f(LeftX, BottomY, Z);
			glTexCoord2f(1, 0);
            glVertex3f(RightX, BottomY, Z);
			glTexCoord2f(1, 1);
            glVertex3f(RightX, TopY, Z);
			
			glTexCoord2f(0, 0);
            glVertex3f(LeftX, BottomY, Z);
			glTexCoord2f(1, 1);
            glVertex3f(RightX, TopY, Z);
			glTexCoord2f(0, 1);
            glVertex3f(LeftX, TopY, Z);
		}
        glEnd();
    }
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
		DebugDrawLine(P0, P1, Circle.Colour);
	}
}
