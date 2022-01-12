/* date = November 12th 2021 5:17 am */

#ifndef PLORE_VIMGUI_H
#define PLORE_VIMGUI_H

#define PLORE_WIDGET_COLOURS \
PLORE_X(Default,      0xff111111, 0xff161616, 0xff1c1c1c) \
PLORE_X(Primary,      0xff111111, 0xff131313, 0xff181818) \
PLORE_X(Secondary,    0xff191933, 0xff191933, 0xff26263f) \
PLORE_X(Tertiary,     0xff66667f, 0xff000000, 0xffffffff) \
PLORE_X(Quaternary,   0xff667f66, 0xff000000, 0xffffffff) \
PLORE_X(RowPrimary,   0xff242424, 0xff3f3f3f, 0xff4c4c4c) \
PLORE_X(RowSecondary, 0xff191933, 0xff19194c, 0xff191966) \
PLORE_X(RowTertiary,  0xff193319, 0xff194c19, 0xff194c19) \
PLORE_X(Window,       0xff0e0e0e, 0xff121212, 0xff181818) \
PLORE_X(Black,        0x00000000, 0x00000000, 0x00000000)

#define PLORE_X(Name, _Ignored1, _Ignored2, _Ignored3) WidgetColour_##Name,
typedef enum widget_colour {
	PLORE_WIDGET_COLOURS
	WidgetColour_Count,
	_WidgetColour_ForceU64 = 0xFFFFFFFF,
} widget_colour;
#undef PLORE_X

#define PLORE_X(_Ignored1, RGBA, _Ignored2, _Ignored3) RGBA,
global u32 PloreWidgetColours[] = {
	PLORE_WIDGET_COLOURS
};
#undef PLORE_X

#define PLORE_X(_Ignored1, _Ignored2, RGBA, _Ignored3) RGBA,
global u32 PloreHotWidgetColours[] = {
	PLORE_WIDGET_COLOURS
};
#undef PLORE_X

#define PLORE_X(_Ignored1, _Ignored2, _Ignored3, RGBA) RGBA,
global u32 PloreFocusWidgetColours[] = {
	PLORE_WIDGET_COLOURS
};
#undef PLORE_X

typedef enum widget_colour_flags {
	WidgetColourFlags_Default,
	WidgetColourFlags_Normal,
	WidgetColourFlags_Hot,
	WidgetColourFlags_Focus,
	_WidgetColourFlags_ForceU64 = 0xFFFFFFFF,
} widget_colour_flags;


#define PLORE_TEXT_COLOURS \
PLORE_X(Default,      0xffffffff, 0xff000000) \
PLORE_X(Primary,      0xffc09061, 0xff997f00) \
PLORE_X(PrimaryFade,  0x6fc09061, 0xff997f00) \
PLORE_X(Secondary,    0xffc0d0ff, 0xff7f7f00) \
PLORE_X(SecondaryFade,0x6fc0d0ff, 0xff7f7f00) \
PLORE_X(Tertiary,     0xff806255, 0xff7f7f00) \
PLORE_X(Prompt,       0xff19d800, 0xff7f7f00) \
PLORE_X(PromptCursor, 0xff4c4cff, 0xff7f7f00) \
PLORE_X(CursorInfo,   0xff6666ff, 0xff7f7f00) \
PLORE_X(TabActive,    0xff6666ff, 0xff7f7f00) \
PLORE_X(Tab,          0xff6666ff, 0xff7f7f00) \
PLORE_X(Lister,       0xffffffff, 0xff7f7f00) 

#define PLORE_X(Name, _Ignored1, _Ignored2) TextColour_##Name,
typedef enum text_colour {
	PLORE_TEXT_COLOURS
	TextColour_Count,
	_TextColour_ForceU64 = 0xFFFFFFFF,
} text_colour;
#undef PLORE_X

typedef enum text_colour_flags {
	TextColourFlags_Default,
	TextColourFlags_Dark,
	_TextColourFlags_ForceU64 = 0xFFFFFFFF,
} text_colour_flags;

#define PLORE_X(Name, RGBA, _Ignored) RGBA,
global u32 PloreTextColours[] = {
	PLORE_TEXT_COLOURS
};
#undef PLORE_X

#define PLORE_X(Name, _Ignored, RGBA) RGBA,
global u32 PloreDarkTextColours[] = {
	PLORE_TEXT_COLOURS
};
#undef PLORE_X

// NOTE(Evan): Container for other widgets.
typedef struct vimgui_window {
	u64 ID;
	rectangle Rect;
	v2 Pad;
	widget_colour BackgroundColour;
	text_colour TextColour;
	char *Title;
	
	// NOTE(Evan): For row layout mode, all per-frame.
	f32 HeightLeft;
	f32 HeightTotal;
	u64 RowCount;
	
	u64 Layer; // NOTE(Evan): Per-frame
	
	b64 Hidden;
	b64 NeverFocus;
} vimgui_window;

typedef enum widget_type {
	WidgetType_Window,
	WidgetType_Button,
	WidgetType_Image,
	WidgetType_TextBox,
	WidgetType_Count,
	_WidgetType_ForceU64 = 0xFFFFFFFF,
} widget_type;

typedef enum vimgui_label_alignment {
	VimguiLabelAlignment_Default,
	VimguiLabelAlignment_Left,
	VimguiLabelAlignment_Right,
	VimguiLabelAlignment_Center,
	VimguiLabelAlignment_CenterHorizontal,
	VimguiLabelAlignment_CenterVertical,
	VimguiLabelAlignment_Count,
	_VimguiLabelAlignment_ForceU64 = 0xffffffffull,
} vimgui_label_alignment;

typedef struct vimgui_label_desc {
	char *Text;
	vimgui_label_alignment Alignment;
	text_colour Colour;
	text_colour_flags ColourFlags;
	v2 Pad;
} vimgui_label_desc;

typedef struct vimgui_widget {
	u64 ID;
	u64 WindowID;
	u64 Layer;
	widget_type Type;
	rectangle Rect;
	widget_colour BackgroundColour;
	widget_colour_flags BackgroundColourFlags;
	
	widget_colour BorderColour;
	widget_colour_flags BorderColourFlags;
	
	vimgui_label_desc Title;
	vimgui_label_desc Secondary;
	vimgui_label_alignment Alignment;
	
	platform_texture_handle Texture; // NOTE(Evan): Images only.
	
	char *Text; // NOTE(Evan): TextBoxes only.
} vimgui_widget;

typedef struct vimgui_widget_state {
	u64 ID;
	u64 TextBoxScroll;
} vimgui_widget_state;

typedef struct plore_vimgui_context {
	struct plore_vimgui_context_this_frame {
		u64 WindowFocusStolen;
		u64 WidgetFocusStolen;
		u64 ParentStackCount;
		u64 ParentStack[8];
		u64 ParentStackMax;
		u64 WindowWeAreLayingOut;
		keyboard_and_mouse Input;
	} ThisFrame;
	plore_map Widgets;
	plore_map WidgetState;
	
	b64 GUIPassActive;
	b64 LayoutPassActive;
	u64 HotWidgetID;
	
	plore_map Windows;
	u64 ActiveWindow;
	u64 HotWindow;
	
	plore_render_list *RenderList;
	plore_font *Font;
	memory_arena FrameArena;
	
	v2 WindowDimensions;
} plore_vimgui_context;

internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input, v2 WindowDimensions);

internal void
VimguiEnd(plore_vimgui_context *Context);

internal void
PushWidget(plore_vimgui_context *Context, vimgui_window *Parent, vimgui_widget Widget);

//
// (Crappy) render primitives.
// These will eventually be converted into vertex buffers when pushed into the RenderList.
//

typedef struct vimgui_render_text_desc {
	rectangle Rect; 
	v4 _TextColour; 
	u64 TextCount; // @Cleanup
	vimgui_label_desc Text;
	u64 FontID;
} vimgui_render_text_desc;
internal void
PushRenderText(plore_render_list *RenderList, vimgui_render_text_desc Desc);

typedef struct vimgui_render_quad_desc {
	rectangle Rect;
	u32 Colour;
	platform_texture_handle Texture;
} vimgui_render_quad_desc;
internal void
PushRenderQuad(plore_render_list *RenderList, vimgui_render_quad_desc Desc);

typedef struct vimgui_render_line_desc {
	v2 P0;
	v2 P1;
	u32 Colour;
} vimgui_render_line_desc;
internal void
PushRenderLine(plore_render_list *RenderList, vimgui_render_line_desc Line);

typedef struct vimgui_render_quarter_circle_desc {
	v2 P;
	f32 R;
	u32 Colour;
	quarter_circle_quadrant Quadrant;
	b64 DrawOutline;
} vimgui_render_quarter_circle_desc;
internal void
PushRenderQuarterCircle(plore_render_list *RenderList, vimgui_render_quarter_circle_desc Desc);

typedef struct vimgui_render_scissor_desc {
	rectangle Rect;
} vimgui_render_scissor_desc;
internal void
PushRenderScissor(plore_render_list *RenderList, vimgui_render_scissor_desc Desc);
	
typedef struct vimgui_window_search_result {
	u64 ID;
	vimgui_window *Window;
} vimgui_window_search_result;
internal vimgui_window *
GetWindow(plore_vimgui_context *Context, u64 ID);

internal vimgui_widget *
GetWidget(plore_vimgui_context *Context, u64 ID);

internal vimgui_window *
GetActiveWindow(plore_vimgui_context *Context);
	
#endif //PLORE_VIMGUI_H
