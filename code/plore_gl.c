#include "plore_gl.h"

internal void
DrawSquare(render_quad Quad) {
	v4 Colour = V4(1, 0, 0, 1);//Quad.Colour;
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	f32 W = Quad.Span.W;
	f32 H = Quad.Span.H;
	f32 X = Quad.P.X;
	f32 Y = Quad.P.Y;
	f32 Z = Quad.Z;
	
    f32 LeftX   = X - W/2.0f;
    f32 RightX  = X + W/2.0f;
    f32 BottomY = Y - H/2.0f;
    f32 TopY    = Y + H/2.0f;
	
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
internal void
DrawTriangle(render_triangle T) {
	glBegin(GL_TRIANGLES);
	glColor4f(T.Colour.R, T.Colour.B, T.Colour.G, T.Colour.A);
	glVertex3f(T.P0.X, T.P0.Y, T.Z);
	glVertex3f(T.P0.X, T.P0.Y, T.Z);
	glVertex3f(T.P0.X, T.P0.Y, T.Z);
	glEnd();
}