internal void
VimguiInit(plore_vimgui_context *Context, plore_render_list *RenderList) {
	Assert(RenderList->Font);
	Context->RenderList = RenderList;
}

internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input, v2 WindowDimensions) {
	Assert(!Context->GUIPassActive);
	Context->GUIPassActive = true;
	Context->ThisFrame = (struct plore_vimgui_context_this_frame) {0};
	
	Context->ThisFrame.Input = Input;
	Context->RenderList->QuadCount = 0;
	Context->RenderList->TextCount = 0;
	
	Context->WindowDimensions = WindowDimensions;
	
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *Window = Context->Windows + W;
		Window->RowCountThisFrame = 0;
		Window->Layer = 0;
	}
}

internal void
VimguiEnd(plore_vimgui_context *Context) {
	Assert(Context->GUIPassActive);
	Context->GUIPassActive = false;
	
	vimgui_widget *Widgets = Context->ThisFrame.Widgets;
	Assert(Context->ThisFrame.WidgetCount);
	
	u64 ActiveWindowID = Context->ActiveWindow;
	
	// NOTE(Evan): Push focused widgets (including window) to the top.
	for (u64 W = 0; W < Context->ThisFrame.WidgetCount; W++) {
		if (Widgets[W].WindowID == ActiveWindowID) {
			Widgets[W].Layer += 2;
		}
	}
#define PloreSortPredicate(A, B) A.Layer < B.Layer
	PloreSort(Context->ThisFrame.Widgets, Context->ThisFrame.WidgetCount, vimgui_widget)
#undef PloreSortPredicate
	
	// NOTE(Evan): Push widgets back-to-front onto the list for painters' algorithm.
	for (u64 W = 0; W < Context->ThisFrame.WidgetCount;  W++) {
		vimgui_widget *Widget = Context->ThisFrame.Widgets + Context->ThisFrame.WidgetCount-W-1;
		
		vimgui_window *Window = GetWindow(Context, Widget->ID);
		
		v4 TextColour = {0};
		v4 BackgroundColour = {0};
		switch (Widget->Type) {
			case WidgetType_Button: {
				switch (Widget->BackgroundColour) {
					case WidgetColour_Default:
					case WidgetColour_Primary: {
						BackgroundColour = V4(0.10, 0.1, 0.1, 1.0);
					} break;
					
					case WidgetColour_Secondary: {
						BackgroundColour = V4(0.2, 0.1, 0.1, 1);
					}; break;
					
					case WidgetColour_Tertiary: {
						BackgroundColour = V4(0.5, 0.4, 0.4, 1);
					} break;
					
					case WidgetColour_Quaternary: {
						BackgroundColour = V4(0.40, 0.5, 0.4, 1);
					} break;
				}
				switch (Widget->TextColour) {
					case TextColour_Default: {
						TextColour = V4(1, 1, 1, 1);
					} break;
					
					case TextColour_Primary: {
						TextColour = V4(0.4, 0.5, 0.6, 1);
					} break;
					
					case TextColour_Secondary: {
						TextColour = V4(0.9, 0.85, 0.80, 1);
					} break;
					
					case TextColour_PrimaryFade: {
						TextColour = V4(0.4, 0.5, 0.6, 0.3);
					} break;
					
					case TextColour_SecondaryFade: {
						TextColour = V4(0.9, 0.85, 0.80, 0.3);
					} break;
					
					case TextColour_Prompt: {
						TextColour = V4(0.2, 0.85, 0.10, 1.0);
					} break;
					
					case TextColour_CursorInfo: {
						TextColour = V4(0.8, 0.4, 0.4, 1);
					} break;
					
				}
				
			} break;
			
			case WidgetType_Window: {
				switch (Widget->BackgroundColour) {
					case WidgetColour_Default: {
						BackgroundColour = V4(0.0, 0.0, 0.0, 1.0);
					} break;
					
					case WidgetColour_Primary: {
					    BackgroundColour = V4(0.05, 0.05, 0.05, 1.0);
					} break;
					
						
				}
				
				switch (Widget->TextColour) {
					case TextColour_Default: {
						TextColour = V4(1, 1, 1, 1);;
					} break;
					case TextColour_Primary:
					case TextColour_Secondary: {
						Assert(!"Unhandled colours.");
					}
				}
			} break;
			
			default: break;
		}
		
		
		if (Context->ActiveWindow == Widget->WindowID) {
			BackgroundColour.RGB = MultiplyVec3f(BackgroundColour.RGB, 1.40f);
		} else if (Context->HotWindow == Widget->WindowID) {
			BackgroundColour.RGB = MultiplyVec3f(BackgroundColour.RGB, 1.10f);
		}
		
		PushRenderQuad(Context->RenderList, (vimgui_render_quad_desc) { 
						   .Rect = Widget->Rect, 
						   .Colour = BackgroundColour, 
						   .Texture = Widget->Texture,
					   });
		
		if (Widget->Title) {
			PushRenderText(Context->RenderList, 
						   (vimgui_render_text_desc) {
						       .Rect = Widget->Rect,
						       .TextColour = TextColour,
						       .Text = Widget->Title, 
						       .Centered = Widget->Centered, 
							   .Height = 32.0f,
						       .TextPad = Widget->TextPad,
						   });
		}
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
		if (MaybeWindow->ID == Context->ThisFrame.WindowWeAreLayingOut) {
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

typedef struct vimgui_image_desc {
	u64 ID;
	platform_texture_handle Texture;
	rectangle Rect;
	b64 Centered;
} vimgui_image_desc;

internal b64
Image(plore_vimgui_context *Context, vimgui_image_desc Desc) {
	b64 Result = true;
	
	Assert(Desc.ID);
	u64 MyID = Desc.ID;
	
	vimgui_window *Window = GetLayoutWindow(Context);
	
	if (IsWithinRectangleInclusive(Context->ThisFrame.Input.MouseP, Desc.Rect)) {
		SetHotWindow(Context, Window);
		Context->HotWidgetID = MyID;
		if (Context->ThisFrame.Input.pKeys[PloreKey_MouseLeft]) {
			SetActiveWindow(Context, Window);
			Context->ActiveWidgetID = MyID;
			Result = true;
		}
	}
	
	Desc.Rect.P = AddVec2(Desc.Rect.P, Window->Rect.P);
	if (Desc.Centered) {
		Desc.Rect.P.X += (Window->Rect.Span.X - Desc.Rect.Span.X) / 2.0f;
		Desc.Rect.P.Y += (Window->Rect.Span.Y - Desc.Rect.Span.X) / 2.0f;
	}
	
	PushWidget(Context, Window, (vimgui_widget) {
				   .Layer = Window->Layer + 1,
				   .Type = WidgetType_Image,
				   .ID = MyID,
				   .WindowID = Window->ID,
				   .Rect = Desc.Rect,
				   .Texture = Desc.Texture,
			   });
	return(Result);
}

typedef struct vimgui_button_desc {
	b64 FillWidth;
	b64 Centre;
	char *Title;
	u64 ID;
	rectangle Rect;
	widget_colour BackgroundColour;
	text_colour TextColour;
	v2 TextPad;
} vimgui_button_desc;

internal b64
Button(plore_vimgui_context *Context, vimgui_button_desc Desc) {
	// NOTE(Evan): We require an ID from the title right now, __LINE__ shenanigans may be required.
	Assert(Desc.Title); 
	Assert(Context->ThisFrame.WindowWeAreLayingOut);
	
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
	
	
	if (Desc.FillWidth) {
		f32 TitlePad = 30.0f;
		f32 ButtonStartY = Window->Rect.P.Y + TitlePad;
		f32 RowPad = 4.0f;
		f32 RowHeight = Desc.Rect.Span.H;
		
		
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
	
	if (IsWithinRectangleInclusive(Context->ThisFrame.Input.MouseP, Desc.Rect)) {
		SetHotWindow(Context, Window);
		Context->HotWidgetID = MyID;
		if (Context->ThisFrame.Input.pKeys[PloreKey_MouseLeft]) {
			SetActiveWindow(Context, Window);
			Context->ActiveWidgetID = MyID;
			Result = true;
		}
	}
	
	Window->RowCountThisFrame++;
	
	PushWidget(Context, Window,
			   (vimgui_widget) {
				   .Type = WidgetType_Button,
				   .ID = MyID,
				   .Layer = Window->Layer + 1,
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
	widget_colour BackgroundColour;
	text_colour TextColour;
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
		Assert(Context->ThisFrame.ParentStackCount < ArrayCount(Context->ThisFrame.ParentStack));
		MaybeWindow->Generation++; // NOTE(Evan): Touch the window so it continues to live.
		if (Desc.Hidden) {
			MaybeWindow->Hidden = true;
			return(false); 
		}
		
		
		u64 MyParentIndex = Context->ThisFrame.ParentStackCount;
		Context->ThisFrame.ParentStack[Context->ThisFrame.ParentStackCount++] = MaybeWindow->ID;
		Context->ThisFrame.ParentStackMax = Max(Context->ThisFrame.ParentStackCount, Context->ThisFrame.ParentStackMax);
		MaybeWindow->Layer = MyParentIndex+1;
		
		// NOTE(Evan): Only check if the active window needs updating if focus wasn't stolen.
		if (!Context->ThisFrame.WindowFocusStolen || Desc.ForceFocus) {
			if (IsWithinRectangleInclusive(Context->ThisFrame.Input.MouseP, Desc.Rect) && !Context->HotWindow) {
				Context->HotWindow = MyID;
			} else if (Desc.ForceFocus) {
				Context->ActiveWindow = MyID;
				Context->ThisFrame.WindowFocusStolen = true;
			}
				
		}
		MaybeWindow->Rect = Desc.Rect;
		
		// NOTE(Evan): If index is 0, then we are the top-level window, and shouldn't offset ourselves
		vimgui_window *Parent = 0;
		if (MyParentIndex) {
			MyParentIndex -= 1;
			Parent = GetWindow(Context, Context->ThisFrame.ParentStack[MyParentIndex]);
			MaybeWindow->Rect.P.X += Parent->Rect.P.X;
			MaybeWindow->Rect.P.Y += Parent->Rect.P.Y;
			MaybeWindow->Rect.P.Y += 32.0f; // NOTE(Evan): This offsets child widgets.
		} 
		MaybeWindow->RowMax = (Desc.Rect.Span.Y) / (36.0f) - 1; // @Hardcode
		MaybeWindow->BackgroundColour = Desc.BackgroundColour;
		Context->ThisFrame.WindowWeAreLayingOut = MyID;
		
		PushWidget(Context, Parent, 
				   (vimgui_widget) {
					   .Type = WidgetType_Window,
					   .ID = MyID,
					   .WindowID = MaybeWindow->ID,
					   .Layer = Parent ? Parent->Layer + 1 : 0,
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
	if (Context->ThisFrame.ParentStackCount) {
		u64 Window = Context->ThisFrame.ParentStack[--Context->ThisFrame.ParentStackCount-1];
		Context->ThisFrame.WindowWeAreLayingOut = Window;
	}
}

internal void
PushWidget(plore_vimgui_context *Context, vimgui_window *Parent, vimgui_widget Widget) {
	Assert(Context->ThisFrame.WidgetCount < ArrayCount(Context->ThisFrame.Widgets));
	rectangle ParentRect = { .Span = Context->WindowDimensions };
	if (Parent) ParentRect = Parent->Rect;
	Widget.Rect = ClipRect(Widget.Rect, ParentRect);
	Context->ThisFrame.Widgets[Context->ThisFrame.WidgetCount++] = Widget;
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
PushRenderQuad(plore_render_list *RenderList, vimgui_render_quad_desc Desc) {
	Assert(RenderList && RenderList->QuadCount < ArrayCount(RenderList->Quads));
	
	RenderList->Quads[RenderList->QuadCount++] = (render_quad) {
		.Rect = Desc.Rect,
		.Colour = Desc.Colour,
		.Texture = Desc.Texture,
	};
}

