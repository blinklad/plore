/* date = November 12th 2021 5:17 am */

#ifndef PLORE_VIMGUI_H
#define PLORE_VIMGUI_H


typedef enum widget_colour {
	WidgetColour_Default,
	WidgetColour_Primary,
	WidgetColour_Secondary,
	WidgetColour_Tertiary,
	WidgetColour_Quaternary,
	WidgetColour_Count,
	_WidgetColour_ForceU64 = 0xFFFFFFFF,
} widget_colour;

typedef enum text_colour {
	TextColour_Default,
	TextColour_Primary,
	TextColour_PrimaryFade,
	TextColour_Secondary,
	TextColour_SecondaryFade,
	TextColour_Tertiary,
	TextColour_TertiaryFade,
	TextColour_Prompt,
	TextColour_CursorInfo,
	TextColour_Count,
	TextColour_Lister,
	_TextColour_ForceU64 = 0xFFFFFFFF,
} text_colour;


// NOTE(Evan): Container for other widgets. These are garbage collected when inactive.
typedef struct vimgui_window {
	u64 ID;
	rectangle Rect;
	v2 Pad;
	widget_colour BackgroundColour;
	text_colour TextColour;
	char *Title;
	u64 RowMax;
	u64 RowCountLastFrame;
	u64 RowCountThisFrame; // NOTE(Evan): ThisFrame
	u64 Layer;             // NOTE(Evan): ThisFrame
	i64 Generation; // NOTE(Evan): When a window is "touched", this is incremented.
	                // If the generation lags behind the global context, the window is deleted. 
	b64 Hidden;
} vimgui_window;

typedef enum widget_type {
	WidgetType_Window,
	WidgetType_Button,
	WidgetType_Image,
	WidgetType_Count,
	_WidgetType_ForceU64 = 0xFFFFFFFF,
} widget_type;

typedef enum vimgui_label_alignment {
	VimguiLabelAlignment_Default,
	VimguiLabelAlignment_Left,
	VimguiLabelAlignment_Right,
	VimguiLabelAlignment_Center,
	VimguiLabelAlignment_Count,
	_VimguiLabelAlignment_ForceU64 = 0xffffffff,
} vimgui_label_alignment;

typedef struct vimgui_label_desc {
	char *Text;
	vimgui_label_alignment Alignment;
	text_colour Colour;
	v2 Pad;
} vimgui_label_desc;

typedef struct vimgui_widget {
	u64 ID;
	u64 WindowID;
	u64 Layer;
	widget_type Type;
	rectangle Rect;
	widget_colour BackgroundColour;
	vimgui_label_desc Title;
	vimgui_label_desc Secondary;
	vimgui_label_alignment Alignment;
	platform_texture_handle Texture;
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
		vimgui_widget Widgets[512];
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
	v4 _TextColour; 
	vimgui_label_desc Text;
	f32 Height;
} vimgui_render_text_desc;

// @Cleanup
internal void
PushRenderText(plore_render_list *RenderList, vimgui_render_text_desc Desc);

typedef struct vimgui_render_quad_desc {
	rectangle Rect;
	v4 Colour;
	platform_texture_handle Texture;
} vimgui_render_quad_desc;

// @Cleanup
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
