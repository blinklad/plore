internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input) {
	Assert(!Context->GUIPassActive);
	Context->GUIPassActive = true;
	
	Context->InputThisFrame = Input;
	Context->RenderList = (plore_render_list) {0};
	
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *Window = Context->Windows + W;
		Window->RowCountThisFrame = 0;
	}
}

internal void
VimguiEnd(plore_vimgui_context *Context) {
	Assert(Context->GUIPassActive);
	Context->GUIPassActive = false;
	Context->WindowWeAreLayingOut = 0;
	
	// NOTE(Evan): Cleanup any windows that didn't have any activity this frame.
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *Window = Context->Windows + W;
		Window->RowCountLastFrame = Window->RowCountThisFrame;
		if (!Window->RowCountThisFrame) {
			Context->Windows[--Context->WindowCount] = *Window;
		}
	}
}

internal vimgui_window *
GetLayoutWindow(plore_vimgui_context *Context) {
	vimgui_window *Window = 0;
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *MaybeWindow = Context->Windows + W;
		if (MaybeWindow->ID == Context->WindowWeAreLayingOut) {
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
			Window = MaybeWindow;
			break;
		}
	}
	
	return(Window);
}

internal void
SetActiveWindow(plore_vimgui_context *Context, vimgui_window *Window) {
	Context->ActiveWindow = Window->ID;
}

internal void
SetHotWindow(plore_vimgui_context *Context, vimgui_window *Window) {
	Context->HotWindow = Window->ID;
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
	vimgui_window *Window = GetLayoutWindow(Context);
	rectangle MyRect = {0};
	v4 MyColour = V4(1, 1, 1, 0.1);
	
	f32 TitlePad = 20.0f;
	f32 ButtonStartY = Window->Rect.P.Y + TitlePad;
	f32 RowPad = 4.0f;
	f32 RowHeight = 36.0f;
	
	if (!Desc.FillWidth) {
		Assert(!"We don't support absolute button positions right now.");
		return false;
	}
	
	if (Window->RowCountThisFrame > Window->RowMax) {
		PrintLine("Maximum number of rows reached for Window %s", Window->Title);
		return false;
	}

	MyRect.Span = (v2) {
		.W = Window->Rect.Span.X,
		.H = RowHeight - RowPad,
	};
	
	MyRect.P = (v2) {
		.X = Window->Rect.P.X,
		.Y = ButtonStartY + Window->RowCountThisFrame*RowHeight+1,
	};
	
	if (IsWithinRectangleInclusive(Context->InputThisFrame.MouseP, MyRect)) {
		SetHotWindow(Context, Window);
		Context->HotWidgetID = MyID;
		if (Context->InputThisFrame.MouseLeftIsPressed) {
			SetActiveWindow(Context, Window);
			Context->ActiveWidgetID = MyID;
			Result = true;
		}
	}
	
	u64 MyRow = Window->RowCountThisFrame++;
	#if 0
	if (MyRow == Window->Cursor) {
		MyColour.A += 0.2f;
		
		if (Window->CursorMovementThisFrame) {
			Context->ActiveWidgetID = MyID;
			Result = true;
		}
	}
	#endif
	
	
	PushRenderQuad(&Context->RenderList, MyRect, MyColour);
	
	PushRenderText(&Context->RenderList, 
				   MyRect,
				   MyColour,
				   Desc.Title, 
				   false);
	
	return(Result);
}

typedef struct vimgui_window_desc {
	u64 ID;
	char *Title;
	rectangle Rect;
	v4 Colour;
} vimgui_window_desc;

internal b64
Window(plore_vimgui_context *Context, vimgui_window_desc Desc) {
	rectangle Rect = Desc.Rect;
	v4 Colour = Desc.Colour;
	char *Title = Desc.Title;
	
	u64 MyID; 
	if (!Title) {
		Assert(Desc.ID);
		MyID = Desc.ID;
	} else {
		MyID = (u64) Title;
	}
	
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
		
		if (Context->ActiveWindow == MyID) {
			Colour.RGB = MultiplyVec3f(Colour.RGB, 1.20f);
		}
		
		MaybeWindow->Rect = Rect;
		MaybeWindow->Rect.P.Y += 80.0f;
		MaybeWindow->RowMax = (Rect.Span.Y) / 32.0f; // @Hardcode
		MaybeWindow->Colour = Colour;
		
		PushRenderQuad(&Context->RenderList, Rect, Colour);
		
		PushRenderText(&Context->RenderList, 
					   Rect,
					   WHITE_V4,
					   Title, 
					   true);
		
		Context->WindowWeAreLayingOut = MyID;
	}
	
	return(!!MaybeWindow);
	
}




internal void
PushRenderText(plore_render_list *RenderList, rectangle Rect, v4 Colour, char *Text, b64 Centered) {
	Assert(RenderList && RenderList->TextCount < ArrayCount(RenderList->Text));
	render_text *T = RenderList->Text + RenderList->TextCount++;
	
	
	v2 TextP = {
		.X = Rect.P.X + (Centered ? Rect.Span.W/2.0f : 0),
		.Y = Rect.P.Y + 26.0f,                            // TODO(Evan): Move font metric handling to Vimgui.
	};
	
	*T = (render_text ) {
		.P = TextP,
		.Colour = WHITE_V4,
		.Centered = Centered,
	};
	if (*Text) {
		StringPrintSized(T->Text, ArrayCount(T->Text), Text);
	}
}

internal void
PushRenderQuad(plore_render_list *RenderList, rectangle Rect, v4 Colour) {
	Assert(RenderList && RenderList->QuadCount < ArrayCount(RenderList->Quads));
	
	RenderList->Quads[RenderList->QuadCount++] = (render_quad) {
		.Rect = Rect,
		.Colour = Colour,
	};
}

