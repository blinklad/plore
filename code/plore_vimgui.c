// TODO(Evan): Metaprogram tables.
internal v4
GetTextColour(text_colour TextColour, text_colour_flags Flags) {
	Assert(TextColour < TextColour_Count);
	v4 Result = {0};
	v4 *Table = PloreTextColours;
	if (Flags == TextColourFlags_Dark) {
		Table = PloreDarkTextColours;
	} else if (Flags == TextColourFlags_Fade) {
		Table = PloreFadeTextColours;
	}
	
	Result = Table[TextColour];
	return(Result);
}

// TODO(Evan): Metaprogram tables.
internal v4
GetWidgetColour(widget_colour WidgetColour, widget_colour_flags Flags) {
	Assert(WidgetColour < WidgetColour_Count);
	v4 Result = {0};
	v4 *Table = PloreWidgetColours;
	if (Flags == WidgetColourFlags_Hot) {
		Table = PloreHotWidgetColours;
	} else if (Flags == WidgetColourFlags_Focus) {
		Table = PloreFocusWidgetColours;
	}
	
	Result = Table[WidgetColour];
	return(Result);
}


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
	
#define PloreSortPredicate(A, B) A.Layer < B.Layer
	PloreSort(Context->ThisFrame.Widgets, Context->ThisFrame.WidgetCount, vimgui_widget)
#undef PloreSortPredicate
	
	// NOTE(Evan): Push widgets back-to-front onto the list for painters' algorithm.
	for (u64 W = 0; W < Context->ThisFrame.WidgetCount;  W++) {
		vimgui_widget *Widget = Context->ThisFrame.Widgets + Context->ThisFrame.WidgetCount-W-1;
		
		vimgui_window *Window = GetWindow(Context, Widget->ID);
		if (!Window) Window = GetWindow(Context, Widget->WindowID);
		
		if (Context->HotWidgetID == Widget->ID ) {
			Widget->BackgroundColourFlags = WidgetColourFlags_Hot;
		} else if (Widget->Type == WidgetType_Window && Context->HotWindow == Widget->ID) {
			if (!Widget->BackgroundColourFlags) Widget->BackgroundColourFlags = WidgetColourFlags_Hot;
		}
		
		v4 BackgroundColour = GetWidgetColour(Widget->BackgroundColour, Widget->BackgroundColourFlags);
		
		PushRenderQuad(Context->RenderList, (vimgui_render_quad_desc) { 
						   .Rect = Widget->Rect, 
						   .Colour = BackgroundColour, 
						   .Texture = Widget->Texture,
					   });
		
		if (Widget->Title.Text) {
			vimgui_label_alignment Alignment = 0;
			if (Widget->Title.Alignment) Alignment = Widget->Title.Alignment;
			else if (Widget->Alignment) Alignment = Widget->Alignment;
			
			//
			// @Cleanup
			//
			PushRenderText(Context->RenderList, 
						   (vimgui_render_text_desc) {
						       .Rect = Widget->Rect,
						       .Text = {
								   .Text = Widget->Title.Text,
								   .Alignment = Alignment,
								   .Colour = Widget->Title.Colour,
								   .Pad = Widget->Title.Pad,
								   .ColourFlags = Widget->Title.ColourFlags,
							   },
							   .Height = 32.0f,
						   });
		}
		
		//
		// @Cleanup
		//
		// @Cutnpaste above, from Widget->Title.Text
		if (Widget->Secondary.Text) {
			vimgui_label_alignment Alignment = 0;
			if (Widget->Secondary.Alignment) Alignment = Widget->Secondary.Alignment;
			else if (Widget->Alignment) Alignment = Widget->Alignment;
			
			if (Alignment == VimguiLabelAlignment_Left || Alignment == VimguiLabelAlignment_Center) {
				if (Widget->Title.Text && 
					(Widget->Title.Alignment == VimguiLabelAlignment_Left) || 
					(Widget->Title.Alignment == VimguiLabelAlignment_Default)) {
					Widget->Rect.P.X += (StringLength(Widget->Title.Text) + 1) * Context->RenderList->Font->Fonts[0].Data[0].xadvance; // @Cleanup
				}
			}
			
			PushRenderText(Context->RenderList, 
						   (vimgui_render_text_desc) {
						       .Rect = Widget->Rect,
						       .Text = {
								   .Text = Widget->Secondary.Text,
								   .Alignment = Alignment,
								   .Colour = Widget->Secondary.Colour,
								   .Pad = Widget->Secondary.Pad,
								   .ColourFlags = Widget->Title.ColourFlags,
							   },
							   .Height = 32.0f,
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
	if (!Window->NeverFocus) {
		Context->HotWindow = Window->ID;
	}
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
	vimgui_label_desc Title;
	vimgui_label_desc Secondary;
	u64 ID;
	rectangle Rect;
	widget_colour BackgroundColour;
	widget_colour_flags BackgroundColourFlags;
} vimgui_button_desc;

internal b64
Button(plore_vimgui_context *Context, vimgui_button_desc Desc) {
	// NOTE(Evan): We require an ID from the title right now, __LINE__ shenanigans may be required.
	Assert(Desc.Title.Text); 
	Assert(Context->ThisFrame.WindowWeAreLayingOut);
	
	b64 Result = false;
	u64 MyID; 
	if (Desc.ID) {
		MyID = Desc.ID;
	} else {
		Assert(Desc.Title.Text);
		MyID = (u64) Desc.Title.Text;
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
				   .BackgroundColourFlags = Desc.BackgroundColourFlags,
				   .Title = Desc.Title,
				   .Secondary = Desc.Secondary,
			   });
	
	
	return(Result);
}

typedef struct vimgui_window_desc {
	u64 ID;
	char *Title;
	rectangle Rect;
	widget_colour BackgroundColour;
	widget_colour_flags BackgroundColourFlags;
	text_colour TextColour;
	b64 ForceFocus;
	b64 NeverFocus;
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
			MaybeWindow->NeverFocus = Desc.NeverFocus;
		}
	}
	
	if (MaybeWindow) {
		Assert(Context->ThisFrame.ParentStackCount < ArrayCount(Context->ThisFrame.ParentStack));
		MaybeWindow->Generation++; // NOTE(Evan): Touch the window so it continues to live.
		if (Desc.Hidden) {
			MaybeWindow->Hidden = true;
			return(false); 
		}
		
		MaybeWindow->NeverFocus = Desc.NeverFocus;
		
		u64 MyParentIndex = Context->ThisFrame.ParentStackCount;
		Context->ThisFrame.ParentStack[Context->ThisFrame.ParentStackCount++] = MaybeWindow->ID;
		Context->ThisFrame.ParentStackMax = Max(Context->ThisFrame.ParentStackCount, Context->ThisFrame.ParentStackMax);
		MaybeWindow->Layer = MyParentIndex+1;
		
		// NOTE(Evan): Only check if the active window needs updating if focus wasn't stolen.
		if (!Context->ThisFrame.WindowFocusStolen || Desc.ForceFocus) {
			if (IsWithinRectangleInclusive(Context->ThisFrame.Input.MouseP, Desc.Rect)) {
				SetHotWindow(Context, MaybeWindow);
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
					   .BackgroundColour = Desc.BackgroundColour,
					   .BackgroundColourFlags = Desc.BackgroundColourFlags,
//					   .TextColour = Desc.TextColour,
					   .Alignment = VimguiLabelAlignment_Center,
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
		.X = Desc.Text.Pad.X + Desc.Rect.P.X,
		.Y = Desc.Text.Pad.Y + Desc.Rect.P.Y + 26.0f,                           
	};
	
	switch (Desc.Text.Alignment) {
		case VimguiLabelAlignment_Default:
		case VimguiLabelAlignment_Left: {
		} break;
		
		case VimguiLabelAlignment_Center: {
			TextP.X += Desc.Rect.Span.W/2.0f;
			TextP.X -= StringLength(Desc.Text.Text)/2.0f * RenderList->Font->Fonts[0].Data[0].xadvance;
		} break;
		
		case VimguiLabelAlignment_Right: {
			f32 TextSpan = StringLength(Desc.Text.Text) * RenderList->Font->Fonts[0].Data[0].xadvance + Desc.Text.Pad.X;
			TextP.X = Desc.Rect.P.X + (Desc.Rect.Span.W - TextSpan);//Desc.Rect.P.X + Desc.Rect.Span.W;
		} break;
		
	} 
	
	*T = (render_text) {
		.Rect = {
			.P = TextP,
			.Span = Desc.Rect.Span,
		},
		.Colour = GetTextColour(Desc.Text.Colour, Desc.Text.ColourFlags),
		.Height = Desc.Height,
	};
	
	if (Desc.Text.Text && *Desc.Text.Text) {
		StringPrintSized(T->Text, ArrayCount(T->Text), Desc.Text.Text);
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


