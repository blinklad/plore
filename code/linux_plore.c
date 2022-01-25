#include "plore.h"
#include "plore_platform.h"

#include "plore_memory.h"
#include "plore_memory.c"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include <stdio.h>

XWindowAttributes xWindowAttributes;
Display *xDisplay;
Window xRoot;
Window xWindow;


int
main(int ArgCount, char **Args) {

	xDisplay = XOpenDisplay(NULL);
	if (!xDisplay) goto Error;

	xRoot = DefaultRootWindow(xDisplay);

	GLint GlAttributes[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	XVisualInfo *xVisualInfo = glXChooseVisual(xDisplay, 0, GlAttributes);
	if (!xVisualInfo) goto Error;

	Colormap xColorMap = XCreateColormap(xDisplay, xRoot, xVisualInfo->visual, AllocNone);
	XSetWindowAttributes xWindowAttributeDesc = {
		.colormap = xColorMap,
		.event_mask = ExposureMask | KeyPressMask,
	};
	xWindow = XCreateWindow(xDisplay,
			xRoot,
			0,
			0,
			1920,
			1080,
			0,
			xVisualInfo->depth,
			InputOutput,
			xVisualInfo->visual,
			CWColormap | CWEventMask,
			&xWindowAttributeDesc);

	XMapWindow(xDisplay, xWindow);
	XStoreName(xDisplay, xWindow, "Plore");

	GLXContext xGLContext = glXCreateContext(xDisplay, xVisualInfo, NULL, GL_TRUE);
	glXMakeCurrent(xDisplay, xWindow, xGLContext);

	glEnable(GL_DEPTH_TEST);


	XEvent xEvent = {0};
	for (;;) {
		XNextEvent(xDisplay, &xEvent);
		if (xEvent.type == Expose) {
			XGetWindowAttributes(xDisplay, xWindow, &xWindowAttributes);
			glViewport(0, 0, xWindowAttributes.width, xWindowAttributes.height);

			{
				glClearColor(1.0, 1.0, 1.0, 1.0);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				glOrtho(-1., 1., -1., 1., 1., 20.);

				glMatrixMode(GL_MODELVIEW);
				glLoadIdentity();
				gluLookAt(0., 0., 10., 0., 0., 0., 0., 1., 0.);
				glBegin(GL_QUADS);
				glColor3f(1., 0., 0.); glVertex3f(-.75, -.75, 0.);
				glColor3f(0., 1., 0.); glVertex3f( .75, -.75, 0.);
				glColor3f(0., 0., 1.); glVertex3f( .75,  .75, 0.);
				glColor3f(1., 1., 0.); glVertex3f(-.75,  .75, 0.);
				glEnd();
			}

			glXSwapBuffers(xDisplay, xWindow);
		} else if (xEvent.type == KeyPress) {
			glXMakeCurrent(xDisplay, None, NULL);
			glXDestroyContext(xDisplay, xGLContext);
			XDestroyWindow(xDisplay, xWindow);
			XCloseDisplay(xDisplay);
		}
	}

Error:
	return(-1);
}
