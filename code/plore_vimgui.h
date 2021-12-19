/* date = November 12th 2021 5:17 am */

#ifndef PLORE_VIMGUI_H
#define PLORE_VIMGUI_H


//
// TODO(Evan): Move to colour #defines, maybe uint32 hex literals?
// NOTE(Evan): Widget Colours.
// Normal, Hot, Focus
//                    R   G   B   A             R   G   B   A,        R,  G,  B,  A
#define PLORE_WIDGET_COLOURS \
PLORE_X(Default,      0.07,  0.07, 0.07,  1.0,   0.09, 0.09, 0.09, 1,  0.11, 0.11, 0.11,  1) \
PLORE_X(Primary,      0.07,  0.07, 0.07,  1.0,   0.09, 0.09, 0.09, 1,  0.11, 0.11, 0.11,  1) \
PLORE_X(Secondary,    0.2,  0.1,   0.1,  1.0,   0.2,  0.1,  0.1,  1,  1,    1,    1,     1) \
PLORE_X(Tertiary,     0.5,  0.4,   0.4,  1.0,   0,  0,  0,  1,        1,    1,    1,     1) \
PLORE_X(Quaternary,   0.4,  0.5,   0.4,  1.0,   0,  0,  0,  1,        1,    1,    1,     1) \
PLORE_X(RowPrimary,   0.16, 0.16,  0.16, 1.0,   0.25, 0.25, 0.25, 1.0,   0.3, 0.3,  0.3,  1)  \
PLORE_X(RowSecondary, 0.2,  0.1,   0.1,  1.0,   0.3, 0.1, 0.1, 1,  0.4,  0.1,  0.1,   1) \
PLORE_X(RowTertiary,  0.1,  0.2,   0.1,  1.0,   0.1, 0.3, 0.1, 1,  0.1,  0.3,  0.1,   1) \
PLORE_X(Window,       0,    0,     0,    1.0,   0.05, 0.05, 0.05, 1,  0.14, 0.14, 0.14,  1) 

#define PLORE_X(Name, _R, _G, _B, _A, _hR, _hG, _hB, _hA, _fR, _fG, _fB, _fA) WidgetColour_##Name,
typedef enum widget_colour {
	PLORE_WIDGET_COLOURS
	WidgetColour_Count,
	_WidgetColour_ForceU64 = 0xFFFFFFFF,
} widget_colour;
#undef PLORE_X

#define PLORE_X(Name, R, G, B, A, _hR, _hG, _hB, _hA, _fR, _fG, _fB, _fA) { R, G, B, A },
global v4 PloreWidgetColours[] = {
	PLORE_WIDGET_COLOURS
};
#undef PLORE_X

#define PLORE_X(Name, _R, _G, _B, _A, hR, hG, hB, hA, _fR, _fG, _fB, _fA) { hR, hG, hB, hA },
global v4 PloreHotWidgetColours[] = {
	PLORE_WIDGET_COLOURS
};
#undef PLORE_X

#define PLORE_X(Name, _R, _G, _B, _A, _hR, _hG, _hB, _hA, fR, fG, fB, fA) { fR, fG, fB, fA },
global v4 PloreFocusWidgetColours[] = {
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

//
//
// TODO(Evan): Move to colour #defines, maybe uint32 hex literals?
// NOTE(Evan): Text Colours.
//
//      Name    Light R    G     B       Fade Alpha   Dark R    G    B    A
#define PLORE_TEXT_COLOURS                                                       \
PLORE_X(Default,      1,   1,    1,      0.3,              0,   0,   0,   1.0f)  \
PLORE_X(Primary,      0.4, 0.5,  0.6,    0.3f,             0.4, 0.5, 0.6, 1.0f)  \
PLORE_X(Secondary,    0.9, 0.85, 0.80,   0.3,                0.5, 0.5, 0.5, 1.0f)  \
PLORE_X(Tertiary,     0.4, 0.4,  0.70,   0.4,              0.5, 0.5, 0.5, 1.0f)  \
PLORE_X(Prompt,       0.2, 0.85, 0.10,   1.0,              0.5, 0.5, 0.5, 1.0f)  \
PLORE_X(PromptCursor, 0.8, 0.3,  0.3,    1,                0.5, 0.5, 0.5, 1.0f)  \
PLORE_X(CursorInfo,   0.8, 0.4,  0.4,    1,                0.5, 0.5, 0.5, 1.0f)  \
PLORE_X(TabActive,    0.8, 0.4,  0.4,    1,                0.5, 0.5, 0.5, 1.0f)  \
PLORE_X(Tab,          0.8, 0.4,  0.4,    0.4,              0.5, 0.5, 0.5, 1.0f)  \
PLORE_X(Lister,       1.0, 1.0,  1.0,    0.5,              0.5, 0.5, 0.5, 1.0f)

#define PLORE_X(Name, _R, _G, _B, _A, _dR, _dG, _dB, _dA) TextColour_##Name,
typedef enum text_colour {
	PLORE_TEXT_COLOURS
	TextColour_Count,
	_TextColour_ForceU64 = 0xFFFFFFFF,
} text_colour;
#undef PLORE_X

typedef enum text_colour_flags {
	TextColourFlags_Default,
	TextColourFlags_Dark,
	TextColourFlags_Fade,
	_TextColourFlags_ForceU64 = 0xFFFFFFFF,
} text_colour_flags;

#define PLORE_X(Name, R, G, B, _A, _dR, _dG, _dB, _dA) { R, G, B, 1 },
global v4 PloreTextColours[] = {
	PLORE_TEXT_COLOURS
};
#undef PLORE_X

#define PLORE_X(Name, R, G, B, A, _dR, _dG, _dB, _dA) { R, G, B, A },
global v4 PloreFadeTextColours[] = {
	PLORE_TEXT_COLOURS
};
#undef PLORE_X

#define PLORE_X(Name, _R, _G, _B, _A, dR, dG, dB, dA) { dR, dG, dB, dA },
global v4 PloreDarkTextColours[] = {
	PLORE_TEXT_COLOURS
};
#undef PLORE_X

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
