/* date = November 12th 2021 5:17 am */

#ifndef PLORE_VIMGUI_H
#define PLORE_VIMGUI_H

typedef struct vimgui_window {
	u64 ID;
	v2 P;
	v2 Span;
	v4 Colour;
	char *Title;
} vimgui_window;

typedef struct plore_vimgui_context {
	b64 GUIPassActive;
	b64 LayoutPassActive;
	keyboard_and_mouse InputThisFrame;
	u64 HotWidgetID;
	u64 ActiveWidgetID;
	plore_render_list RenderList;
	
	vimgui_window Windows[8];
	u64 WindowCount;
} plore_vimgui_context;

typedef struct plore_state plore_state;

internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input); 

internal void
VimguiEnd(plore_vimgui_context *Context);

#endif //PLORE_VIMGUI_H