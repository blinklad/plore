/* date = November 12th 2021 5:17 am */

#ifndef PLORE_VIMGUI_H
#define PLORE_VIMGUI_H

// NOTE(Evan): Container for other widgets. These are garbage collected when inactive.
typedef struct vimgui_window {
	u64 ID;
	rectangle Rect;
	v4 Colour;
	char *Title;
	u64 RowMax;
	u64 RowCountThisFrame;
	u64 RowCountLastFrame;
	i64 Generation; // NOTE(Evan): When a window is "touched", this is incremented.
	                // If the generation lags behind the global context, the window is deleted. 
} vimgui_window;

typedef struct vimgui_widget {
	u64 Dummy;
} vimgui_widget;

typedef struct plore_vimgui_context {
	b64 GUIPassActive;
	b64 LayoutPassActive;
	b64 WindowFocusStolenThisFrame;
	b64 WidgetFocusStolenThisFrame;
	
	keyboard_and_mouse InputThisFrame;
	u64 HotWidgetID;
	u64 ActiveWidgetID;
	plore_render_list RenderList;
	
	vimgui_window Windows[8];
	u64 WindowCount;
	u64 ActiveWindow;
	u64 HotWindow;
	u64 WindowWeAreLayingOut;
	i64 GenerationCount;
} plore_vimgui_context;

internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input); 

internal void
VimguiEnd(plore_vimgui_context *Context);

internal void
PushRenderText(plore_render_list *RenderList, rectangle Rect, v4 Colour, char *Text, b64 Centered);

internal void
PushRenderQuad(plore_render_list *RenderList, rectangle Rect, v4 Colour);

typedef struct vimgui_window_search_result {
	u64 ID;
	vimgui_window *Window;
} vimgui_window_search_result;

internal vimgui_window *
GetActiveWindow(plore_vimgui_context *Context);
	
#endif //PLORE_VIMGUI_H
