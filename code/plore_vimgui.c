// TODO(Evan): Metaprogram tables.
internal u32 
GetTextColour(text_colour TextColour, text_colour_flags Flags) {
	Assert(TextColour < TextColour_Count);
	u32 Result = {0};
	u32 *Table = PloreTextColours;
	if (Flags == TextColourFlags_Dark) {
		Table = PloreDarkTextColours;
	}
	
	Result = Table[TextColour];
	return(Result);
}

// TODO(Evan): Metaprogram tables.
internal u32
GetWidgetColour(widget_colour WidgetColour, widget_colour_flags Flags) {
	Assert(WidgetColour < WidgetColour_Count);
	u32 Result = 0;
	u32 *Table = PloreWidgetColours;
	if (Flags == WidgetColourFlags_Hot) {
		Table = PloreHotWidgetColours;
	} else if (Flags == WidgetColourFlags_Focus) {
		Table = PloreFocusWidgetColours;
	}
	
	Result = Table[WidgetColour];
	return(Result);
}

internal vimgui_widget_state *
GetOrCreateWidgetState(plore_vimgui_context *Context, u64 ID);

internal void
VimguiInit(memory_arena *Arena, plore_vimgui_context *Context, plore_render_list *RenderList) {
	Assert(RenderList->Font);
	Context->RenderList = RenderList;
	Context->Font = RenderList->Font;
	Context->FrameArena = SubArena(Arena, Megabytes(1), 16);
}

internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input, v2 WindowDimensions) {
	Assert(!Context->GUIPassActive);
	Context->GUIPassActive = true;
	Context->ThisFrame = (struct plore_vimgui_context_this_frame) {0};
	
	Context->ThisFrame.Input = Input;
	Context->RenderList->CommandCount = 0;
	
	Context->WindowDimensions = WindowDimensions;
	
	for (u64 W = 0; W < Context->WindowCount; W++) {
		vimgui_window *Window = Context->Windows + W;
		Window->RowCountThisFrame = 0;
		Window->Layer = 0;
	}
	
	ClearArena(&Context->FrameArena);
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
		
		u32 BackgroundColour = GetWidgetColour(Widget->BackgroundColour, Widget->BackgroundColourFlags);
		u32 Border = 0x25ffffff;
		rectangle bRect = Widget->Rect;
		f32 GapPad = 0.0f;
		
		if (Widget->Type == WidgetType_Button) {
			Border = 0x25ffffff;
		}
		
		if (Widget->Layer == 0) {
			Border = 0;
		}
		
		
		bRect.Span.W += GapPad;
		bRect.Span.H += GapPad;
		Widget->Rect.Span.W -= GapPad;
		Widget->Rect.Span.H -= GapPad;
		Widget->Rect.P.X += GapPad;
		Widget->Rect.P.Y += GapPad;
		
		f32 R = 10.0f;
		
		PushRenderLine(Context->RenderList, (vimgui_render_line_desc) { 
						   .P0 = V2(bRect.P.X+R, bRect.P.Y), 
						   .P1 = V2(bRect.P.X+bRect.Span.W-R, bRect.P.Y),
						   .Colour = Border,
					   });
		
		PushRenderLine(Context->RenderList, (vimgui_render_line_desc) { 
						   .P0 = V2(bRect.P.X, bRect.P.Y+R), 
						   .P1 = V2(bRect.P.X, bRect.P.Y+bRect.Span.H-R),
						   .Colour = Border,
					   });
		
		PushRenderLine(Context->RenderList, (vimgui_render_line_desc) { 
						   .P0 = V2(bRect.P.X+bRect.Span.W, bRect.P.Y+R), 
						   .P1 = V2(bRect.P.X+bRect.Span.W, bRect.P.Y+bRect.Span.H-R),
						   .Colour = Border,
					   });
		
		PushRenderLine(Context->RenderList, (vimgui_render_line_desc) { 
						   .P0 = V2(bRect.P.X+R, bRect.P.Y+bRect.Span.H), 
						   .P1 = V2(bRect.P.X+bRect.Span.W-R, bRect.P.Y+bRect.Span.H),
						   .Colour = Border,
					   });
		
		PushRenderQuarterCircle(Context->RenderList, (vimgui_render_quarter_circle_desc) {
									.P = V2(bRect.P.X+R, bRect.P.Y+R), 
									.R = R,
									.Colour = Border,
									.Quadrant = QuarterCircleQuadrant_TopLeft,
								});
		
		
		PushRenderQuad(Context->RenderList, (vimgui_render_quad_desc) { 
						   .Rect = Widget->Rect, 
						   .Colour = BackgroundColour, 
						   .Texture = Widget->Texture,
					   });
		
		
		f32 FontHeight = GetCurrentFontHeight(Context->Font);
		f32 FontWidth = GetCurrentFontWidth(Context->Font);
		u64 MaxTextCols = Widget->Rect.Span.W / FontWidth;
		u64 MaxTextRows = Widget->Rect.Span.H / FontHeight;
		
		//
		// @Cleanup
		// NOTE(Evan): Dodgy text layout ahead!
		// Text length truncation within a single widget works for these combinations: { 1. title: left, 2. title: right, 3. title: left, secondary: right }
		//
		u64 TextCount = 0;
		if (Widget->Secondary.Text) {
			u64 PotentialTextCount = StringLength(Widget->Secondary.Text);
			u64 MaxTextCount = MaxTextCols - TextCount;
			
			if (PotentialTextCount <= MaxTextCount) {
				TextCount += PotentialTextCount;
				vimgui_label_alignment Alignment = 0;
				if (Widget->Secondary.Alignment) Alignment = Widget->Secondary.Alignment;
				else if (Widget->Alignment) Alignment = Widget->Alignment;
				
				rectangle SecondaryRect = Widget->Rect;
				if (Alignment == VimguiLabelAlignment_Left || Alignment == VimguiLabelAlignment_Center) {
					if (Widget->Title.Text && 
						(Widget->Title.Alignment == VimguiLabelAlignment_Left) || 
						(Widget->Title.Alignment == VimguiLabelAlignment_Default)) {
						SecondaryRect.P.X += (StringLength(Widget->Title.Text) + 1) * FontWidth; // @Cleanup
					}
				}
				
				PushRenderText(Context->RenderList, 
							   (vimgui_render_text_desc) {
							       .Rect = SecondaryRect,
							       .Text = {
									   .Text = Widget->Secondary.Text,
									   .Alignment = Alignment,
									   .Colour = Widget->Secondary.Colour,
									   .Pad = Widget->Secondary.Pad,
									   .ColourFlags = Widget->Title.ColourFlags,
								   },
								   .FontID = Context->Font->CurrentFont,
								   });
			}
		}
		if (Widget->Title.Text) {
			// NOTE(Evan): Grab a bigger font for the title.
			u64 TitleFontID = Context->Font->CurrentFont;
			if (Widget->Type == WidgetType_Window) {
				TitleFontID = Min(PloreBakedFont_Count-1, Context->Font->CurrentFont+5);
			}
				
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
							   .FontID = TitleFontID,
							   .TextCount = TextCount,
						   });
			
		}
		
		if (Widget->Type == WidgetType_TextBox) {
			char *Lines = Widget->Text;
			if (Lines) {
				vimgui_widget_state *WidgetState = GetOrCreateWidgetState(Context, Widget->ID);
				u64 StartLine = WidgetState->TextBoxScroll;
				
				if (Widget->ID == Context->HotWidgetID) {
					i64 ScrollDir = Context->ThisFrame.Input.ScrollDirection;
					if (!(ScrollDir < 0 && WidgetState->TextBoxScroll == 0)) {
						WidgetState->TextBoxScroll += ScrollDir;
						
					}
				}
				u64 RenderedLines = 0;
				for (u64 L = 0; L < MaxTextRows; L++) {
					u64 LineCount = 0;
					char *LineStart = Lines;
					while (*Lines) {
						char C = *Lines++;
						if (!C || C == '\n') break;
						
						if (LineCount < MaxTextCols) LineCount += 1;
						
					}
					if (L < StartLine) {
						MaxTextRows += 1;
						continue;
					}
					
					char *ThisLine = PushBytes(&Context->FrameArena, LineCount+1);
					StringCopy(LineStart, ThisLine, LineCount+1);
					
					PushRenderText(Context->RenderList, 
								   (vimgui_render_text_desc) {
									   .Rect = {
										   .P = {
											   Widget->Rect.P.X,
											   Widget->Rect.P.Y + RenderedLines++*FontHeight,
										   },
										   .Span = Widget->Rect.Span,
									   },
									   .Text = {
										   .Text = ThisLine,
										   .Colour = Widget->Title.Colour,
										   //.Pad = Widget->Title.Pad,
										   //.ColourFlags = Widget->Title.ColourFlags,
									   },
									   .FontID = Context->Font->CurrentFont,
								   });
					
					if (!Lines) {
						int BreakHere = 5;
						break;
					}
				}
				
			}
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
	
	Context->HotWidgetID = 0;
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

internal vimgui_widget *
GetWidget(plore_vimgui_context *Context, u64 ID) {
	vimgui_widget *MaybeWidget = 0;
	for (u64 W = 0; W < Context->ThisFrame.WidgetCount; W++) {
		vimgui_widget *Widget = Context->ThisFrame.Widgets + W;
		if (Widget->ID == ID) {
			MaybeWidget = Widget;
			break;
		}
			
	}
	
	return(MaybeWidget);
}

internal vimgui_widget_state *
GetOrCreateWidgetState(plore_vimgui_context *Context, u64 ID) {
	vimgui_widget_state *Result = 0;
	for (u64 S = 0; S < Context->WidgetStateCount; S++) {
		if (Context->WidgetState[S].ID == ID) {
			Result = Context->WidgetState + S;
			break;
		}
	}
	
	if (!Result) {
		if (Context->WidgetStateCount == ArrayCount(Context->WidgetState)) {
			Context->WidgetState[0] = ClearStruct(vimgui_widget_state);
			Result = Context->WidgetState + 0;
		} else {
			Result = Context->WidgetState + Context->WidgetStateCount++;
		}
		Result->ID = ID;
	}
	
	return(Result);
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

typedef struct vimgui_hit_test_result {
	b64 Hot;
	b64 Active;
} vimgui_hit_test_result;

// NOTE(Evan): Assumes the rectangle is already adjusted to be parent-relative!
internal vimgui_hit_test_result
DoHitTest(plore_vimgui_context *Context, vimgui_window *Window, u64 ID, rectangle Rect) {
	vimgui_hit_test_result Result = {0};
	
	if (IsWithinRectangleInclusive(Context->ThisFrame.Input.MouseP, Rect)) {
		SetHotWindow(Context, Window);
		Context->HotWidgetID = ID;
		Result.Hot = true;
		if (Context->ThisFrame.Input.pKeys[PloreKey_MouseLeft]) {
			SetActiveWindow(Context, Window);
			Result.Active = true;
		}
	}
	
	return(Result);
}

typedef struct vimgui_image_desc {
	u64 ID;
	platform_texture_handle Texture;
	rectangle Rect;
	b64 Centered;
} vimgui_image_desc;

internal void
Image(plore_vimgui_context *Context, vimgui_image_desc Desc) {
	Assert(Desc.ID);
	u64 MyID = Desc.ID;
	
	vimgui_window *Window = GetLayoutWindow(Context);
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
}

typedef struct vimgui_text_box_desc {
	u64 ID;
	char *Text;
	rectangle Rect;
	b64 Centered;
} vimgui_text_box_desc;

internal void
TextBox(plore_vimgui_context *Context, vimgui_text_box_desc Desc) {
	Assert(Desc.ID);
	u64 MyID = Desc.ID;
	
	vimgui_window *Window = GetLayoutWindow(Context);
	Desc.Rect.P = AddVec2(Desc.Rect.P, Window->Rect.P);
	
	vimgui_hit_test_result HitResult = DoHitTest(Context, Window, MyID, Desc.Rect);
	if (Desc.Centered) {
		Desc.Rect.P.X += (Window->Rect.Span.X - Desc.Rect.Span.X) / 2.0f;
		Desc.Rect.P.Y += (Window->Rect.Span.Y - Desc.Rect.Span.X) / 2.0f;
	}
	
	vimgui_widget_state *State = GetOrCreateWidgetState(Context, MyID);
	if (HitResult.Hot) {
		if (!(Context->ThisFrame.Input.ScrollDirection < 0 && State->TextBoxScroll == 0)) {
			State->TextBoxScroll += Context->ThisFrame.Input.ScrollDirection;
		}
	}
	
	PushWidget(Context, Window, (vimgui_widget) {
				   .Layer = Window->Layer + 1,
				   .Type = WidgetType_TextBox,
				   .ID = MyID,
				   .WindowID = Window->ID,
				   .Rect = Desc.Rect,
				   .Text = Desc.Text,
			   });
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
	
	vimgui_hit_test_result Result = {0};
	u64 MyID; 
	if (Desc.ID) {
		MyID = Desc.ID;
	} else {
		Assert(Desc.Title.Text);
		MyID = (u64) Desc.Title.Text;
	}
	
	vimgui_window *Window = GetLayoutWindow(Context);
	if (Window->RowCountThisFrame > Window->RowMax) return false;
	
	// NOTE(Evan): Default args.
	
	if (Desc.FillWidth) {
		f32 FontHeight = GetCurrentFontHeight(Context->Font);
		f32 TitlePad = FontHeight+4.0f;
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
		Window->RowCountThisFrame++;
		
	} else {
		Desc.Rect.P.X += Window->Rect.P.X;
		Desc.Rect.P.Y += Window->Rect.P.Y;
	}
	
	Result = DoHitTest(Context, Window, MyID, Desc.Rect);
	
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
	
	
	return(Result.Active);
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
		
		// NOTE(Evan): If index is 0, then we are the top-level window, and shouldn't offset ourselves
		vimgui_window *Parent = 0;
		if (MyParentIndex) {
			MyParentIndex -= 1;
			Parent = GetWindow(Context, Context->ThisFrame.ParentStack[MyParentIndex]);
			MaybeWindow->Rect.P.X += Parent->Rect.P.X;
			MaybeWindow->Rect.P.Y += Parent->Rect.P.Y;
			MaybeWindow->Rect.P.Y += 26.0f; // NOTE(Evan): This offsets child widgets, currently it must account for tabs - cleanup!
		} 
		
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
		
		MaybeWindow->RowMax = (Desc.Rect.Span.Y) / (GetCurrentFontHeight(Context->Font) + 4.0f) - 1; // @Hardcode
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
					   .Alignment = VimguiLabelAlignment_CenterHorizontal,
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
	Assert(RenderList && RenderList->CommandCount < ArrayCount(RenderList->Commands));
	
	f32 FontHeight = GetCurrentFontHeight(RenderList->Font);
	f32 FontWidth = GetCurrentFontWidth(RenderList->Font);
	
	u64 MaxTextCount = Desc.Rect.Span.W / FontWidth - Desc.TextCount;
	u64 ThisTextLength = StringLength(Desc.Text.Text);
	if (ThisTextLength >= MaxTextCount-1) {
		Desc.Text.Text[MaxTextCount] = '\0';
	}
	
	v2 TextP = {
		.X = Desc.Text.Pad.X + Desc.Rect.P.X,
		.Y = Desc.Text.Pad.Y + Desc.Rect.P.Y + (FontHeight*0.8125f),
	};
	
	switch (Desc.Text.Alignment) {
		case VimguiLabelAlignment_Default:
		case VimguiLabelAlignment_Left: {
		} break;
		
		case VimguiLabelAlignment_Center: {
			TextP.X += Desc.Rect.Span.W/2.0f;
			TextP.X -= StringLength(Desc.Text.Text)/2.0f * FontWidth;
			TextP.Y += Desc.Rect.Span.H/2.0f-FontHeight/2;
		} break;
		case VimguiLabelAlignment_CenterVertical: {
			TextP.Y += Desc.Rect.Span.H/2.0f;
		} break;
		case VimguiLabelAlignment_CenterHorizontal: {
			TextP.X += Desc.Rect.Span.W/2.0f;
			TextP.X -= StringLength(Desc.Text.Text)/2.0f * FontWidth;
		} break;
		
		case VimguiLabelAlignment_Right: {
			f32 TextSpan = StringLength(Desc.Text.Text) * FontWidth + Desc.Text.Pad.X;
			TextP.X = Desc.Rect.P.X + (Desc.Rect.Span.W - TextSpan);//Desc.Rect.P.X + Desc.Rect.Span.W;
		} break;
		
	} 
	
	render_command *C = RenderList->Commands + RenderList->CommandCount++;
	*C = (render_command) {
		.Type = RenderCommandType_Text,
		.Text = (render_text) {
			.Rect = {
				.P = TextP,
				.Span = Desc.Rect.Span,
			},
			.Colour = ColourU32ToV4(GetTextColour(Desc.Text.Colour, Desc.Text.ColourFlags)),
			.Texture = RenderList->Font->Handles[Desc.FontID],
			.Data = RenderList->Font->Data[Desc.FontID],
		},
	};
	
	if (Desc.Text.Text && *Desc.Text.Text) {
		StringPrintSized(C->Text.Text, ArrayCount(C->Text.Text), "%s", Desc.Text.Text);
	}
}

internal void
PushRenderQuad(plore_render_list *RenderList, vimgui_render_quad_desc Desc) {
	Assert(RenderList && RenderList->CommandCount < ArrayCount(RenderList->Commands));
	
	RenderList->Commands[RenderList->CommandCount++] = (render_command) {
		.Type = RenderCommandType_PrimitiveQuad,
		.Quad = {
			.Rect = Desc.Rect,
			.Colour = ColourU32ToV4(Desc.Colour),
			.Texture = Desc.Texture,
		},
	};
}

internal void
PushRenderLine(plore_render_list *RenderList, vimgui_render_line_desc Desc) {
	Assert(RenderList && RenderList->CommandCount < ArrayCount(RenderList->Commands));
	
	RenderList->Commands[RenderList->CommandCount++] = (render_command) {
		.Type = RenderCommandType_PrimitiveLine,
		.Line = {
			.P0 = Desc.P0,
			.P1 = Desc.P1,
			.Colour = ColourU32ToV4(Desc.Colour),
		},
	};
}

internal void
PushRenderQuarterCircle(plore_render_list *RenderList, vimgui_render_quarter_circle_desc Desc) {
	Assert(RenderList && RenderList->CommandCount < ArrayCount(RenderList->Commands));
	
	RenderList->Commands[RenderList->CommandCount++] = (render_command) {
		.Type = RenderCommandType_PrimitiveQuarterCircle,
		.QuarterCircle = {
			.P = Desc.P,
			.R = Desc.R,
			.Colour = ColourU32ToV4(Desc.Colour),
			.Quadrant = Desc.Quadrant,
			.DrawOutline = Desc.DrawOutline,
		},
	};
}


internal void
PushRenderScissor(plore_render_list *RenderList, vimgui_render_scissor_desc Desc) {
	Assert(RenderList && RenderList->CommandCount < ArrayCount(RenderList->Commands));
	
	RenderList->Commands[RenderList->CommandCount++] = (render_command) {
		.Type = RenderCommandType_Scissor,
		.Scissor = {
			.Rect = Desc.Rect,
		},
	};
}
	