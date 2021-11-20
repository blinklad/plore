/* date = November 12th 2021 5:17 am */

#ifndef PLORE_VIMGUI_H
#define PLORE_VIMGUI_H

// NOTE(Evan): Container for other widgets. These are garbage collected when inactive.
typedef struct vimgui_window {
	u64 ID;
	rectangle Rect;
	v2 Pad;
	v4 Colour;
	char *Title;
	u64 RowMax;
	u64 RowCountThisFrame;
	u64 RowCountLastFrame;
	i64 Generation; // NOTE(Evan): When a window is "touched", this is incremented.
	                // If the generation lags behind the global context, the window is deleted. 
	b64 Hidden;
} vimgui_window;

typedef enum vimgui_widget_type {
	VimguiWidgetType_Window,
	VimguiWidgetType_Button,
	_VimguiWidgetType_ForceU64 = 0xFFFFFFFF,
} vimgui_widget_type;

typedef struct vimgui_widget {
	u64 ID;
	u64 WindowID;
	vimgui_widget_type Type;
	rectangle Rect;
	v4 Colour;
	v4 TextColour;
	char *Title;
	b64 Centered;
} vimgui_widget;

typedef struct plore_vimgui_context {
	b64 GUIPassActive;
	b64 LayoutPassActive;
	b64 WindowFocusStolenThisFrame;
	b64 WidgetFocusStolenThisFrame;
	
	keyboard_and_mouse InputThisFrame;
	u64 HotWidgetID;
	u64 ActiveWidgetID;
	
	vimgui_window Windows[8];
	u64 WindowCount;
	u64 ActiveWindow;
	u64 HotWindow;
	u64 WindowWeAreLayingOut;
	
	u64 ParentStack[8];
	u64 ParentStackCount;
	
	i64 GenerationCount;
	
	vimgui_widget Widgets[512];
	u64 WidgetCount;
	plore_render_list *RenderList;
	
	v2 WindowDimensions;
} plore_vimgui_context;

internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input, v2 WindowDimensions);

internal void
VimguiEnd(plore_vimgui_context *Context);

internal void
PushWidget(plore_vimgui_context *Context, vimgui_window *Parent, vimgui_widget Widget);
	
internal void
PushRenderText(plore_render_list *RenderList, rectangle Rect, v4 Colour, char *Text, b64 Centered, f32 Height);

internal void
PushRenderQuad(plore_render_list *RenderList, rectangle Rect, v4 Colour);

typedef struct vimgui_window_search_result {
	u64 ID;
	vimgui_window *Window;
} vimgui_window_search_result;

internal vimgui_window *
GetActiveWindow(plore_vimgui_context *Context);
	
#endif //PLORE_VIMGUI_H
