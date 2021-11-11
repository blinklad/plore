/* date = November 12th 2021 5:17 am */

#ifndef PLORE_VIMGUI_H
#define PLORE_VIMGUI_H

typedef struct vimgui_window {
	u64 ID;
	rectangle Rect;
	v4 Colour;
	char *Title;
} vimgui_window;

typedef struct plore_vimgui_context {
	b64 GUIPassActive;
	b64 LayoutPassActive;
	b64 WindowPassActive; // NOTE(Evan): This means, "Are we laying out a window right now?"
	
	keyboard_and_mouse InputThisFrame;
	u64 HotWidgetID;
	u64 ActiveWidgetID;
	plore_render_list RenderList;
	
	vimgui_window Windows[8];
	u64 WindowCount;
	vimgui_window *ActiveWindow;
	vimgui_window *HotWindow;
} plore_vimgui_context;

typedef struct plore_state plore_state;

internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input); 

internal void
VimguiEnd(plore_vimgui_context *Context);

internal void
PushRenderText(plore_render_list *RenderList, v2 P, v4 Colour, char *Text, b64 Centered);

internal void
PushRenderQuad(plore_render_list *RenderList, rectangle Rect, v4 Colour);


#endif //PLORE_VIMGUI_H
