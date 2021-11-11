
internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input) {
	Assert(!Context->GUIPassActive);
	Context->GUIPassActive = true;
	
	Context->InputThisFrame = Input;
	Context->RenderList = (plore_render_list) {0};
	
	if (Input.LIsPressed) {
		vimgui_window_search_result MaybeActiveWindow = GetRightMovementWindow(Context);
		if (MaybeActiveWindow.ID) {
			Context->ActiveWindow = MaybeActiveWindow.ID;
			Context->HotWindow = MaybeActiveWindow.ID;
		}
	} else if (Input.HIsPressed) {
		vimgui_window_search_result MaybeActiveWindow = GetLeftMovementWindow(Context);
		if (MaybeActiveWindow.ID) {
			Context->ActiveWindow = MaybeActiveWindow.ID;
			Context->HotWindow = MaybeActiveWindow.ID;
		}
	} else if (Input.JIsPressed) {
	} else if (Input.KIsPressed) {
	}
	
}

internal void
VimguiEnd(plore_vimgui_context *Context) {
	Assert(Context->GUIPassActive);
	Context->GUIPassActive = false;
}

internal vimgui_window_search_result
GetMovementWindow(plore_vimgui_context *Context, i64 Sign) {
	vimgui_window_search_result Result = {0};
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *MaybeWindow = Context->Windows + W;
		if (MaybeWindow->ID == Context->ActiveWindow) {
			if ((Sign < 0 && W > 0) || (Sign > 0 && W < Context->WindowCount-1)) {
				Result.Window = Context->Windows + W + Sign;
				Result.ID = (u64) Result.Window->Title;
			} else {
				Result.Window = MaybeWindow;
				Result.ID = (u64) Result.Window->Title;
			}
			break;
		}
	}
	return(Result);
}

internal vimgui_window_search_result
GetRightMovementWindow(plore_vimgui_context *Context) {
	vimgui_window_search_result Result = GetMovementWindow(Context, +1);
	return(Result);
}
	
internal vimgui_window_search_result
GetLeftMovementWindow(plore_vimgui_context *Context) {
	vimgui_window_search_result Result = GetMovementWindow(Context, -1);
	return(Result);
}

internal vimgui_window *
GetActiveWindow(plore_vimgui_context *Context) {
	vimgui_window *Window = 0;
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *MaybeWindow = Context->Windows + W;
		if (MaybeWindow->ID == Context->ActiveWindow) {
			Window = MaybeWindow;
			break;
		}
	}
	
	return(Window);
}

typedef struct vimgui_button_desc {
	char *Title;
	rectangle Rect;
	b64 FillWidth;
} vimgui_button_desc;

internal b64
Button(plore_vimgui_context *Context, vimgui_button_desc Desc) {
	// NOTE(Evan): We require an ID from the title right now, __LINE__ shenanigans may be required.
	Assert(Desc.Title); 
	Assert(Context->ActiveWindow);
	
	u64 MyID = (u64) Desc.Title;
	vimgui_window *Window = GetActiveWindow(Context);
	if (IsWithinRectangleInclusive(Context->InputThisFrame.MouseP, Window->Rect)) {
	}
}

internal b64
WindowTitled(plore_vimgui_context *Context, char *Title, rectangle Rect, v4 Colour) {
	u64 MyID = (u64) Title;
	vimgui_window *MaybeWindow = 0;
	
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *Window = Context->Windows + W;
		if (Window->ID == MyID) {
			MaybeWindow = Window;
			break;
		}
	}
	
	if (!MaybeWindow) {
		if (Context->WindowCount < ArrayCount(Context->Windows)) {
			MaybeWindow = Context->Windows + Context->WindowCount++;
			MaybeWindow->ID = MyID;
			MaybeWindow->Title = Title;
		}
	}
	
	if (MaybeWindow) {
		b64 WeAreTheOnlyWindow = Context->WindowCount == 1;
		if (WeAreTheOnlyWindow) {
			Context->ActiveWindow = MyID;
			Context->HotWindow = MyID;
		} else if (IsWithinRectangleInclusive(Context->InputThisFrame.MouseP, Rect) && !Context->HotWindow) {
			Context->ActiveWindow = MyID;
		}
		
		if (Context->HotWindow == MyID) {
			Colour.RGB = MultiplyVec3f(Colour.RGB, 1.20f);
		}
		MaybeWindow->Rect = Rect;
		MaybeWindow->Colour = Colour;
		
		PushRenderQuad(&Context->RenderList, Rect, Colour);
		
		PushRenderText(&Context->RenderList, 
					   RectangleTopCentre(Rect),
					   WHITE_V4,
					   Title, 
					   true);
		
	}
	
	return(!!MaybeWindow);
	
}




internal void
PushRenderText(plore_render_list *RenderList, v2 P, v4 Colour, char *Text, b64 Centered) {
	Assert(RenderList && RenderList->TextCount < ArrayCount(RenderList->Text));
	render_text *T = RenderList->Text + RenderList->TextCount++;
	*T = (render_text ) {
		.P = P,
		.Colour = WHITE_V4,
		.Centered = Centered,
	};
	StringPrintSized(T->Text, ArrayCount(T->Text), Text);
}

internal void
PushRenderQuad(plore_render_list *RenderList, rectangle Rect, v4 Colour) {
	Assert(RenderList && RenderList->QuadCount < ArrayCount(RenderList->Quads));
	
	RenderList->Quads[RenderList->QuadCount++] = (render_quad) {
		.Rect = Rect,
		.Colour = Colour,
	};
}

