
internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input) {
	Assert(!Context->GUIPassActive);
	Context->GUIPassActive = true;
	
	Context->InputThisFrame = Input;
	Context->RenderList = (plore_render_list) {0};
	
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *Window = Context->Windows + W;
		Window->RowCount = 0;
	}
	
	if (Input.CtrlIsDown) {
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
	
}

internal void
VimguiEnd(plore_vimgui_context *Context) {
	Assert(Context->GUIPassActive);
	Context->GUIPassActive = false;
	Context->WindowWeAreLayingOut = 0;
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
GetTheWindowWeAreLayingOut(plore_vimgui_context *Context) {
	vimgui_window *Window = 0;
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *MaybeWindow = Context->Windows + W;
		if (MaybeWindow->ID == Context->WindowWeAreLayingOut) {
			PrintLine("Window we are laying out index :: %d", W);
			Window = MaybeWindow;
			break;
		}
	}
	
	return(Window);
}

internal vimgui_window *
GetHotWindow(plore_vimgui_context *Context) {
	vimgui_window *Window = 0;
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *MaybeWindow = Context->Windows + W;
		if (MaybeWindow->ID == Context->HotWindow) {
			PrintLine("Hot window index :: %d", W);
			Window = MaybeWindow;
			break;
		}
	}
	
	return(Window);
}

internal vimgui_window *
GetActiveWindow(plore_vimgui_context *Context) {
	vimgui_window *Window = 0;
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *MaybeWindow = Context->Windows + W;
		if (MaybeWindow->ID == Context->ActiveWindow) {
			PrintLine("Active window index :: %d", W);
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
	b64 Centre;
	v4 Colour;
} vimgui_button_desc;

internal b64
Button(plore_vimgui_context *Context, vimgui_button_desc Desc) {
	// NOTE(Evan): We require an ID from the title right now, __LINE__ shenanigans may be required.
	Assert(Desc.Title); 
	Assert(Context->WindowWeAreLayingOut);
	
	b64 Result = false;
	u64 MyID = (u64) Desc.Title;
	vimgui_window *Window = GetTheWindowWeAreLayingOut(Context);
	rectangle MyRect = {0};
	v4 MyColour = V4(1, 1, 1, 0.3);
	
	if (Desc.FillWidth) {
		f32 ButtonStartY = Window->Rect.Centre.Y - Window->Rect.HalfSpan.H + 20.0f;
		MyRect.HalfSpan = (v2) {
			.W = Window->Rect.HalfSpan.X,
			.H = 16.0f,
		};
		
		MyRect.Centre = (v2) {
			.X = Window->Rect.Centre.X,
			.Y = ButtonStartY + Window->RowCount*32.0f,
		};
		
		Assert(Window->Rect.Centre.Y - Window->Rect.HalfSpan.H > 0.0f);
	} else {
		MyRect = Desc.Rect;
	}
	
	if (Context->WindowWeAreLayingOut == Context->ActiveWindow) {
		if (IsWithinRectangleInclusive(Context->InputThisFrame.MouseP, MyRect)) {
			Context->HotWidgetID = MyID;
			if (Context->InputThisFrame.MouseLeftIsPressed) {
				Context->ActiveWidgetID = MyID;
				Result = true;
			}
		}
	}
	PushRenderQuad(&Context->RenderList, MyRect, MyColour);
	
	rectangle UpsideDownRect = MyRect;
	UpsideDownRect.Centre.Y = Window->Rect.HalfSpan.Y*2-(Window->RowCount++)*32.0f-80.0f;
	if (Desc.Centre) {
		UpsideDownRect.Centre.X -= Window->Rect.HalfSpan.X;
	}
	
	PushRenderText(&Context->RenderList, 
				   RectangleTopCentre(UpsideDownRect),
				   MyColour,
				   Desc.Title, 
				   false);
	
	return(Result);
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
		} else {
		}
		
		if (Context->HotWindow == MyID) {
			Colour.RGB = MultiplyVec3f(Colour.RGB, 1.20f);
		}
		
		MaybeWindow->Rect = Rect;
		MaybeWindow->Rect.Centre.Y += 80.0f;
		MaybeWindow->RowMax = (Rect.HalfSpan.Y * 2.0f) / 32.0f; // @Hardcode
		MaybeWindow->Colour = Colour;
		
		PushRenderQuad(&Context->RenderList, Rect, Colour);
		
		PushRenderText(&Context->RenderList, 
					   RectangleTopCentre(Rect),
					   WHITE_V4,
					   Title, 
					   true);
		
		Context->WindowWeAreLayingOut = MyID;
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

