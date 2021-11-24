internal void
VimguiInit(plore_vimgui_context *Context, plore_render_list *RenderList) {
	Assert(RenderList->Font);
	Context->RenderList = RenderList;
}

internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input, v2 WindowDimensions) {
	Assert(!Context->GUIPassActive);
	Context->GUIPassActive = true;
	
	Context->InputThisFrame = Input;
	
	Context->RenderList->QuadCount = 0;
	Context->RenderList->TextCount = 0;
	Context->WidgetCount = 0;
	
	Context->WindowFocusStolenThisFrame = false;
	Context->WidgetFocusStolenThisFrame = false;
	
	Context->ParentStackCount = 0;
	Context->WindowWeAreLayingOut = 0;
	Context->WindowDimensions = WindowDimensions;
	
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *Window = Context->Windows + W;
		Window->RowCountThisFrame = 0;
	}
}

internal void
VimguiEnd(plore_vimgui_context *Context) {
	Assert(Context->GUIPassActive);
	Context->GUIPassActive = false;
	
	vimgui_window *Window = GetActiveWindow(Context);
	
	// TODO(Evan): When we buffer widgets, build the render list here, so we can associate
	// e.g. bitmaps, z-ordering, etc with draw commands, rather then just quads + text.
	for (u64 W = 0; W < Context->WidgetCount;  W++) {
		vimgui_widget *Widget = Context->Widgets + W;
		
		if (Context->ActiveWindow == Widget->ID) {
			Widget->BackgroundColour = V4(0.15, 0.15, 0.15, 1.0f);
		} else if (Context->HotWindow == Widget->ID) {
			Widget->BackgroundColour.RGB = MultiplyVec3f(Widget->BackgroundColour.RGB, 1.10f);
		}
		
		PushRenderQuad(Context->RenderList, Widget->Rect, Widget->BackgroundColour);
		
		PushRenderText(Context->RenderList, 
					   (vimgui_render_text_desc) {
					       .Rect = Widget->Rect,
					       .TextColour = Widget->TextColour,
					       .Text = Widget->Title, 
					       .Centered = Widget->Centered, 
						   .Height = 32.0f,
					       .TextPad = Widget->TextPad,
					   }
					   );
	}
	
	
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
	u64 ID;
	rectangle Rect;
	v4 BackgroundColour;
	v4 TextColour;
	v2 TextPad;
} vimgui_button_desc;

internal b64
Button(plore_vimgui_context *Context, vimgui_button_desc Desc) {
	// NOTE(Evan): We require an ID from the title right now, __LINE__ shenanigans may be required.
	Assert(Desc.Title); 
	Assert(Context->WindowWeAreLayingOut);
	
	b64 Result = false;
	u64 MyID; 
	if (Desc.ID) {
		MyID = Desc.ID;
	} else {
		Assert(Desc.Title);
		MyID = (u64) Desc.Title;
	}
	
	vimgui_window *Window = GetLayoutWindow(Context);
	if (Window->RowCountThisFrame > Window->RowMax) {
		return false;
	}
	
	// NOTE(Evan): Default args.
	if (MemoryCompare(&Desc.BackgroundColour, &(v4){0}, sizeof(v4)) == 0) {
		Desc.BackgroundColour = V4(0.65, 0.65, 0.65, 0.1);
	}
	
	if (MemoryCompare(&Desc.TextColour, &(v4){0}, sizeof(v4)) == 0) {
		Desc.TextColour = V4(1, 1, 1, 1);
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
	
	PushWidget(Context, Window,
			   (vimgui_widget) {
				   .Type = VimguiWidgetType_Button,
				   .ID = MyID,
				   .WindowID = Window->ID,
				   .Rect = Desc.Rect,
				   .BackgroundColour = Desc.BackgroundColour,
				   .TextColour = Desc.TextColour,
				   .TextPad = Desc.TextPad,
				   .Title = Desc.Title,
			   });
	
	
	return(Result);
}

typedef struct vimgui_window_desc {
	u64 ID;
	char *Title;
	rectangle Rect;
	v4 BackgroundColour;
	v4 TextColour;
	b64 ForceFocus;
	b64 Hidden;
} vimgui_window_desc;

internal b64
Window(plore_vimgui_context *Context, vimgui_window_desc Desc) {
	u64 MyID; 
	if (Desc.ID) {
		MyID = Desc.ID;
	} else {
		Assert(Desc.Title);
		MyID = (u64) Desc.Title;
	}
	
	// NOTE(Evan): Default args.
	if (MemoryCompare(&Desc.BackgroundColour, &(v4){0}, sizeof(v4)) == 0) {
		Desc.BackgroundColour = V4(1, 1, 1, 0.1);
	}
	
	if (MemoryCompare(&Desc.TextColour, &(v4){0}, sizeof(v4)) == 0) {
		Desc.TextColour = V4(1, 1, 1, 1);
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
		MaybeWindow->Generation++; // NOTE(Evan): Touch the window so it continues to live.
		if (Desc.Hidden) {
			MaybeWindow->Hidden = true;
			return(false); 
		}
		
		
		u64 MyParentIndex = Context->ParentStackCount;
		Context->ParentStack[Context->ParentStackCount++] = MaybeWindow->ID;
		
		// NOTE(Evan): Only check if the active window needs updating if focus wasn't stolen.
		if (!Context->WindowFocusStolenThisFrame || Desc.ForceFocus) {
			if (IsWithinRectangleInclusive(Context->InputThisFrame.MouseP, Desc.Rect) && !Context->HotWindow) {
				Context->HotWindow = MyID;
			} else if (Desc.ForceFocus) {
				Context->ActiveWindow = MyID;
				Context->WindowFocusStolenThisFrame = true;
			}
				
		}
		MaybeWindow->Rect = Desc.Rect;
		
		// NOTE(Evan): If index is 0, then we are the top-level window, and shouldn't offset ourselves
		vimgui_window *Parent = 0;
		if (MyParentIndex) {
			MyParentIndex -= 1;
			Parent = GetWindow(Context, Context->ParentStack[MyParentIndex]);
			MaybeWindow->Rect.P.X += Parent->Rect.P.X;
			MaybeWindow->Rect.P.Y += Parent->Rect.P.Y;
			MaybeWindow->Rect.P.Y += 32.0f; // NOTE(Evan): This offsets child widgets.
		} 
		MaybeWindow->RowMax = (Desc.Rect.Span.Y) / (36.0f) - 1; // @Hardcode
		MaybeWindow->Colour = Desc.BackgroundColour;
		Context->WindowWeAreLayingOut = MyID;
		
		PushWidget(Context, Parent, 
				   (vimgui_widget) {
					   .Type = VimguiWidgetType_Window,
					   .ID = MyID,
					   .Rect = MaybeWindow->Rect,
					   .Title = MaybeWindow->Title,
					   .Centered = true, 
					   .BackgroundColour = Desc.BackgroundColour,
					   .TextColour = Desc.TextColour,
					   
				   });
		
	}
	
	return(!!MaybeWindow);
}

// NOTE(Evan): Clips A into B's bounds, inclusive.
plore_inline rectangle
ClipRect(rectangle A, rectangle B) {
	rectangle Result = {
		.P = {
			.X = Clamp(A.P.X, B.P.X, B.P.X + B.Span.X), 
			.Y = Clamp(A.P.Y, B.P.Y, B.P.Y + B.Span.Y), 
		},
		.Span = {
			.W = Clamp(A.Span.X, 0, B.Span.X),
			.H = Clamp(A.Span.Y, 0, B.Span.Y),
		}
	};
	
	return(Result);
}

internal void
WindowEnd(plore_vimgui_context *Context) {
	if (Context->ParentStackCount) {
		u64 Window = Context->ParentStack[--Context->ParentStackCount-1];
		Context->WindowWeAreLayingOut = Window;
	}
}

internal void
PushWidget(plore_vimgui_context *Context, vimgui_window *Parent, vimgui_widget Widget) {
	Assert(Context->WidgetCount < ArrayCount(Context->Widgets));
	rectangle ParentRect = { .Span = Context->WindowDimensions };
	if (Parent) ParentRect = Parent->Rect;
	Widget.Rect = ClipRect(Widget.Rect, ParentRect);
	Context->Widgets[Context->WidgetCount++] = Widget;
}

internal void
PushRenderText(plore_render_list *RenderList, vimgui_render_text_desc Desc) {
	Assert(RenderList && RenderList->TextCount < ArrayCount(RenderList->Text));
	render_text *T = RenderList->Text + RenderList->TextCount++;
	
	
	v2 TextP = {
		.X = Desc.TextPad.X + Desc.Rect.P.X + (Desc.Centered ? Desc.Rect.Span.W/2.0f : 0),
		.Y = Desc.TextPad.Y + Desc.Rect.P.Y + 26.0f,                            // TODO(Evan): Move font metric handling to Vimgui.
	};
	
	*T = (render_text) {
		.Rect = {
			.P = TextP,
			.Span = Desc.Rect.Span,
		},
		.Colour = Desc.TextColour,
		.Centered = Desc.Centered,
		.Height = Desc.Height,
	};
	if (*Desc.Text) {
		StringPrintSized(T->Text, ArrayCount(T->Text), Desc.Text);
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

