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
	u64 RowCountLastFrame;
	u64 RowCountThisFrame; // NOTE(Evan): Temporary
	u64 ParentStackLayer;  // NOTE(Evan): Temporary
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
	v4 BackgroundColour;
	v4 TextColour;
	v2 TextPad;
	char *Title;
	b64 Centered;
} vimgui_widget;

typedef struct plore_vimgui_context {
	struct plore_vimgui_context_this_frame {
		u64 WidgetCount;
		u64 WindowFocusStolen;
		u64 WidgetFocusStolen;
		u64 ParentStackCount;
		u64 ParentStack[8];
		u64 ParentStackMax;
		u64 WindowWeAreLayingOut;
		keyboard_and_mouse Input;
	} ThisFrame;
	
	b64 GUIPassActive;
	b64 LayoutPassActive;
	u64 HotWidgetID;
	u64 ActiveWidgetID;
	
	vimgui_window Windows[8];
	u64 WindowCount;
	u64 ActiveWindow;
	u64 HotWindow;
	
	i64 GenerationCount;
	
	vimgui_widget Widgets[512];
	plore_render_list *RenderList;
	
	v2 WindowDimensions;
} plore_vimgui_context;

internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input, v2 WindowDimensions);

internal void
VimguiEnd(plore_vimgui_context *Context);

internal void
PushWidget(plore_vimgui_context *Context, vimgui_window *Parent, vimgui_widget Widget);
	

typedef struct vimgui_render_text_desc {
	rectangle Rect; 
	v4 TextColour; 
	char *Text; 
	b64 Centered; 
	f32 Height;
	v2 TextPad;
} vimgui_render_text_desc;

internal void
PushRenderText(plore_render_list *RenderList, vimgui_render_text_desc Desc);

typedef struct vimgui_render_quad_desc {
	rectangle Rect;
	v4 Colour;
} vimgui_render_quad_desc;

internal void
PushRenderQuad(plore_render_list *RenderList, vimgui_render_quad_desc Desc);

typedef struct vimgui_window_search_result {
	u64 ID;
	vimgui_window *Window;
} vimgui_window_search_result;


internal vimgui_window *
GetWindow(plore_vimgui_context *Context, u64 ID);

internal vimgui_window *
GetActiveWindow(plore_vimgui_context *Context);
	
#endif //PLORE_VIMGUI_H
