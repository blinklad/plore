internal void
VimguiInit(plore_vimgui_context *Context, plore_font *Font) {
	Context->RenderList.Font = Font;
}

internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input) {
	Assert(!Context->GUIPassActive);
	Context->GUIPassActive = true;
	
	Context->InputThisFrame = Input;
	
	Context->RenderList.QuadCount = 0;
	Context->RenderList.TextCount = 0;
	Context->WidgetCount = 0;
	
	Context->WindowFocusStolenThisFrame = false;
	Context->WidgetFocusStolenThisFrame = false;
	
	Context->ParentStackCount = 0;
	
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
	
	// NOTE(Evan): Cleanup any windows that didn't have any activity the past two frames.
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *Window = Context->Windows + W;
		Assert(Window->ID);
		
		Window->RowCountLastFrame = Window->RowCountThisFrame;
		if (Window->RowCountThisFrame == 0) {
			if (AbsDifference(Window->Generation, Context->GenerationCount) > 1) {
				*Window = Context->Windows[--Context->WindowCount];
			}
		}
	}
	
	// TODO(Evan): When we buffer widgets, build the render list here, so we can associate
	// e.g. bitmaps, z-ordering, etc with draw commands, rather then just quads + text.
	
	for (u64 W = 0; W < Context->WidgetCount;  W++) {
		vimgui_widget *Widget = Context->Widgets + W;
		PushRenderQuad(&Context->RenderList, Widget->Rect, Widget->Colour);
		
		PushRenderText(&Context->RenderList, 
					   Widget->Rect,
					   Widget->Colour,
					   Widget->Title, 
					   Widget->Centered);
	}
	
	// TODO(Evan): Flush font here!
	
	
	Context->GenerationCount++;
	
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


internal vimgui_window *
GetWindow(plore_vimgui_context *Context, u64 ID) {
	vimgui_window *MaybeWindow = 0;
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *Window = Context->Windows + W;
		if (Window->ID == ID) {
			MaybeWindow = Window;
			break;
		}
	}
	
	return(MaybeWindow);
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
	b64 FillWidth;
	b64 Centre;
	char *Title;
	rectangle Rect;
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
	if (MemoryCompare(&Desc.Colour, &(v4){0}, sizeof(v4)) == 0) {
		Desc.Colour = V4(1, 1, 1, 0.1);
	}
	
	if (Window->RowCountThisFrame > Window->RowMax) {
		return false;
	}
	
	if (Desc.FillWidth) {
		Desc.Rect = (rectangle) {0};
		f32 TitlePad = 30.0f;
		f32 ButtonStartY = Window->Rect.P.Y + TitlePad;
		f32 RowPad = 4.0f;
		f32 RowHeight = 36.0f;
		
		Desc.Rect.Span = (v2) {
			.W = Window->Rect.Span.X,
			.H = RowHeight - RowPad,
		};
		
		Desc.Rect.P = (v2) {
			.X = Window->Rect.P.X,
			.Y = ButtonStartY + Window->RowCountThisFrame*RowHeight+1,
		};
	} else {
		Desc.Rect.P.X += Window->Rect.P.X;
		Desc.Rect.P.Y += Window->Rect.P.Y;
	}
	
	if (IsWithinRectangleInclusive(Context->InputThisFrame.MouseP, Desc.Rect)) {
		SetHotWindow(Context, Window);
		Context->HotWidgetID = MyID;
		if (Context->InputThisFrame.MouseLeftIsPressed) {
			SetActiveWindow(Context, Window);
			Context->ActiveWidgetID = MyID;
			Result = true;
		}
	}
	
	Window->RowCountThisFrame++;
	
	PushWidget(Context, (vimgui_widget) {
				   .Type = VimguiWidgetType_Button,
				   .ID = MyID,
				   .WindowID = Window->ID,
				   .Rect = Desc.Rect,
				   .Colour = Desc.Colour,
				   .Title = Desc.Title,
			   });
	
	
	return(Result);
}

typedef struct vimgui_window_desc {
	u64 ID;
	char *Title;
	rectangle Rect;
	v4 Colour;
	b64 ForceFocus;
	b64 Centered;
} vimgui_window_desc;

internal b64
Window(plore_vimgui_context *Context, vimgui_window_desc Desc) {
	u64 MyID; 
	if (!Desc.Title) {
		Assert(Desc.ID);
		MyID = Desc.ID;
	} else {
		MyID = (u64) Desc.Title;
	}
	
	vimgui_window *MaybeWindow = GetWindow(Context, MyID);
	
	
	if (!MaybeWindow) {
		if (Context->WindowCount < ArrayCount(Context->Windows)) {
			MaybeWindow = Context->Windows + Context->WindowCount++;
			*MaybeWindow = (vimgui_window) {0};
			MaybeWindow->ID = MyID;
			MaybeWindow->Title = Desc.Title;
			MaybeWindow->Generation = Context->GenerationCount;
		}
	}
	
	if (MaybeWindow) {
		Assert(Context->ParentStackCount < ArrayCount(Context->ParentStack));
		
		u64 MyParentIndex = Context->ParentStackCount;
		Context->ParentStack[Context->ParentStackCount++] = MaybeWindow->ID;
		MaybeWindow->Generation++;
		
		// NOTE(Evan): Only check if the active window needs updating if focus wasn't stolen.
		if (!Context->WindowFocusStolenThisFrame) {
			b64 WeAreTheOnlyWindow = Context->WindowCount == 1;
			if (WeAreTheOnlyWindow) {
				Context->ActiveWindow = MyID;
				Context->HotWindow = MyID;
			} else if (IsWithinRectangleInclusive(Context->InputThisFrame.MouseP, Desc.Rect) && !Context->HotWindow) {
				Context->ActiveWindow = MyID;
			} else if (Desc.ForceFocus) {
				Context->ActiveWindow = MyID;
				Context->WindowFocusStolenThisFrame = true;
			}
				
		}
		if (Context->ActiveWindow == MyID) {
			Desc.Colour.RGB = MultiplyVec3f(Desc.Colour.RGB, 1.20f);
		}
		
		MaybeWindow->Rect = Desc.Rect;
		
		// NOTE(Evan): If index is 0, then we are the top-level window, and shouldn't offset ourselves
		if (MyParentIndex) {
			MyParentIndex -= 1;
			vimgui_window *Parent = GetWindow(Context, Context->ParentStack[MyParentIndex]);
			MaybeWindow->Rect.P.X += Parent->Rect.P.X;
			MaybeWindow->Rect.P.Y += Parent->Rect.P.Y;
			MaybeWindow->Rect.P.Y += 32.0f; // NOTE(Evan): This offsets child widgets.
		} 
		MaybeWindow->RowMax = (Desc.Rect.Span.Y) / 32.0f; // @Hardcode
		MaybeWindow->Colour = Desc.Colour;
		
		PushWidget(Context, (vimgui_widget) {
					   .Type = VimguiWidgetType_Window,
					   .ID = MyID,
					   .Rect = MaybeWindow->Rect,
					   .Colour = MaybeWindow->Colour,
					   .Title = MaybeWindow->Title,
					   .Centered = Desc.Centered, 
				   });
		
		Context->WindowWeAreLayingOut = MyID;
	}
	
	return(!!MaybeWindow);
}

internal void
WindowEnd(plore_vimgui_context *Context) {
	if (Context->ParentStackCount) Context->ParentStackCount--;
}

internal void
PushWidget(plore_vimgui_context *Context, vimgui_widget Widget) {
	Assert(Context->WidgetCount < ArrayCount(Context->Widgets));
	Context->Widgets[Context->WidgetCount++] = Widget;
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
		.Rect = {
			.P = TextP,
			.Span = Rect.Span,
		},
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

