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
	Context->Windows     = MapInit(Arena, Context->Windows, false, 64);
	Context->Widgets     = MapInit(Arena, Context->Widgets, false, 512);
	Context->WidgetState = MapInit(Arena, Context->WidgetState, false, 16);
}

internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input, v2 WindowDimensions) {
	Assert(!Context->GUIPassActive);
	Context->GUIPassActive = true;
	Context->ThisFrame = (struct plore_vimgui_context_this_frame) {0};

	Context->ThisFrame.Input = Input;
	Context->RenderList->CommandCount = 0;

	Context->WindowDimensions = WindowDimensions;

	for (u64 It = 0;
		 It < MapCapacity(Context->Windows);
		 It += 1) {
		if (!MapIsAllocated(Context->Windows, It)) continue;
		vimgui_window *Window = &Context->Windows[It].V;
		Window->HeightLeft = Window->Rect.Span.H;
		Window->HeightTotal = Window->Rect.Span.H;
		Window->Layer = 0;
	}

	ClearArena(&Context->FrameArena);
	MapReset(Context->Widgets);
}

internal void
VimguiEnd(plore_vimgui_context *Context) {
	Assert(Context->GUIPassActive);
	Context->GUIPassActive = false;

	f32 dX = Context->WindowDimensions.X;
	f32 dY = Context->WindowDimensions.Y;

	Assert(MapCount(Context->Widgets));
	vimgui_widget *Widgets = PushArray(&Context->FrameArena, vimgui_widget, MapCount(Context->Widgets));
	u64 WidgetCount = 0;
	for (u64 It = 0;
		 It < MapCapacity(Context->Widgets);
		 It += 1) {
		if (!MapIsAllocated(Context->Widgets, It)) continue;
		Widgets[WidgetCount++] = Context->Widgets[It].V;
	}


	f32 FontHeight = GetCurrentFontHeight(Context->Font);
	f32 FontWidth = GetCurrentFontWidth(Context->Font);

#define PloreSortPredicate(A, B) A.Layer < B.Layer
	PloreSort(Widgets, WidgetCount, vimgui_widget)
#undef PloreSortPredicate

	// NOTE(Evan): Push widgets back-to-front onto the list for painters' algorithm.
	for (u64 W = 0; W < WidgetCount;  W++) {
		vimgui_widget *Widget = Widgets + WidgetCount-W-1;

		vimgui_window *Window = GetWindow(Context, Widget->ID);
		if (!Window) Window = GetWindow(Context, Widget->WindowID);

		if (Context->HotWidgetID == Widget->ID ) {
			Widget->BackgroundColourFlags = WidgetColourFlags_Hot;
		} else if (Widget->Type == WidgetType_Window && Context->HotWindow == Widget->ID) {
			if (!Widget->BackgroundColourFlags) Widget->BackgroundColourFlags = WidgetColourFlags_Hot;
		}

		// CLEANUP(Evan): Border business is a bit messy.
		u32 BackgroundColour = GetWidgetColour(Widget->BackgroundColour, Widget->BackgroundColourFlags);
		u32 BorderColour = 0x15ffffff;
		rectangle bRect = Widget->Rect;

		f32 R = 2.0f;
		if (Widget->Type == WidgetType_Button) {
			BorderColour = 0x25ffffff;
			if (Widget->BackgroundColourFlags == WidgetColourFlags_Focus) {
				R = (48.0f/FontHeight)*2.0f + 4;
			}
		} else {
			R = 8.0f;
		}
		if (Widget->Layer == 0) BorderColour = 0;

		rectangle P = Window->Rect;
		rectangle S = {
			.P = {
				.X = P.P.X,
				.Y = dY - (P.P.Y + P.Span.H)
			},
			.Span = P.Span,
		};

		PushRenderScissor(Context->RenderList, (vimgui_render_scissor_desc) {
							  .Rect = S,
						  });


		if (Widget->Type == WidgetType_Image) {
			rectangle ThiccRect = Widget->Rect;
			f32 BorderThickness = 6.0f;
			ThiccRect.P.X -= BorderThickness;
			ThiccRect.P.Y -= BorderThickness;
			ThiccRect.Span.W += BorderThickness*2;
			ThiccRect.Span.H += BorderThickness*2;

			PushRenderQuad(Context->RenderList, (vimgui_render_quad_desc) {
							   .Rect = ThiccRect,
							   .Colour = 0xff303030,
						   });
			PushRenderQuad(Context->RenderList, (vimgui_render_quad_desc) {
							   .Rect = Widget->Rect,
							   .Colour = BackgroundColour,
							   .Texture = Widget->Texture,
						   });
		} else {
			bRect.P.X += 1;
			bRect.P.Y += 1;
			bRect.Span.W -= 3;
			bRect.Span.H -= 1;

			rectangle MiddleRect = Widget->Rect;
			{
				MiddleRect.P.X += R;
				MiddleRect.Span.W -= 2*R;
				PushRenderQuad(Context->RenderList, (vimgui_render_quad_desc) {
								   .Rect = MiddleRect,
								   .Colour = BackgroundColour,
							   });

			}
			rectangle LeftRect = Widget->Rect;
			{
				LeftRect.Span.W = R;
				LeftRect.P.Y += R;
				LeftRect.Span.H -= 2*R;

				PushRenderQuad(Context->RenderList, (vimgui_render_quad_desc) {
								   .Rect = LeftRect,
								   .Colour = BackgroundColour,
								   });
			}

			rectangle RightRect = Widget->Rect;
			{
				RightRect.P.X += RightRect.Span.W-R;
				RightRect.Span.W = R;
				RightRect.P.Y += R;
				RightRect.Span.H -= 2*R;

				PushRenderQuad(Context->RenderList, (vimgui_render_quad_desc) {
								   .Rect = RightRect,
								   .Colour = BackgroundColour,
							   });
			}


			typedef struct arc_bits {
				v2 P;
				quarter_circle_quadrant Quadrant;
			} arc_bits;
			arc_bits ArcBits[4] = {
				[0] = {
					.P = V2(bRect.P.X+R, bRect.P.Y+R),
					.Quadrant = QuarterCircleQuadrant_TopLeft,
				},
				[1] = {
					.P = V2(bRect.P.X+R, bRect.P.Y+bRect.Span.H-R),
					.Quadrant = QuarterCircleQuadrant_BottomLeft,
				},


				[2] = {
					.P = V2(bRect.P.X+bRect.Span.W-R, bRect.P.Y+R),
					.Quadrant = QuarterCircleQuadrant_TopRight,
				},


				[3] = {
					.P = V2(bRect.P.X+bRect.Span.W-R, bRect.P.Y+bRect.Span.H-R),
					.Quadrant = QuarterCircleQuadrant_BottomRight,
				},
			};

			for (u64 A = 0; A < ArrayCount(ArcBits); A++) {
				PushRenderQuarterCircle(Context->RenderList, (vimgui_render_quarter_circle_desc) {
											.P = ArcBits[A].P,
											.R = R,
											.Colour = BackgroundColour,
											.Quadrant = ArcBits[A].Quadrant,
										});

				PushRenderQuarterCircle(Context->RenderList, (vimgui_render_quarter_circle_desc) {
											.P = ArcBits[A].P,
											.R = R,
											.Colour = BorderColour,
											.Quadrant = ArcBits[A].Quadrant,
											.DrawOutline = true,
										});
			}

		}

		// NOTE(Evan): Lines lie within the rect as scissor is not inclusive.
		PushRenderLine(Context->RenderList, (vimgui_render_line_desc) {
						   .P0 = V2(bRect.P.X+R, bRect.P.Y),
						   .P1 = V2(bRect.P.X+bRect.Span.W-R, bRect.P.Y),
						   .Colour = BorderColour,
					   });

		PushRenderLine(Context->RenderList, (vimgui_render_line_desc) {
						   .P0 = V2(bRect.P.X, bRect.P.Y+R),
						   .P1 = V2(bRect.P.X, bRect.P.Y+bRect.Span.H-R),
						   .Colour = BorderColour,
					   });

		PushRenderLine(Context->RenderList, (vimgui_render_line_desc) {
						   .P0 = V2(bRect.P.X+bRect.Span.W, bRect.P.Y+R),
						   .P1 = V2(bRect.P.X+bRect.Span.W, bRect.P.Y+bRect.Span.H-R),
						   .Colour = BorderColour,
					   });

		PushRenderLine(Context->RenderList, (vimgui_render_line_desc) {
						   .P0 = V2(bRect.P.X+R, bRect.P.Y+bRect.Span.H),
						   .P1 = V2(bRect.P.X+bRect.Span.W-R, bRect.P.Y+bRect.Span.H),
						   .Colour = BorderColour,
					   });


		u64 MaxTextCols = Widget->Rect.Span.W / FontWidth;
		u64 MaxTextRows = Widget->Rect.Span.H / FontHeight;

		//
		// CLEANUP(Evan): Dodgy text layout ahead!
		// Text length truncation within a single widget works for these combinations:
		// { 1. title: left, 2. title: right, 3. title: left, secondary: right }
		//
		u64 TextCount = 0;
		u64 SmallerFont = Max(0, Context->Font->CurrentFont-6);
		if (SmallerFont > PloreBakedFont_Count-1) {
			SmallerFont = 0;
		}

		if (Widget->Title.Text) {
			// NOTE(Evan): Grab a bigger font for the title.
			rectangle TitleRect = Widget->Rect;
			TitleRect.P.X += 1.0f;
			TitleRect.Span.W -= 2.0f;

			u64 TitleFontID = Context->Font->CurrentFont;
			if (Widget->Type == WidgetType_Window) {
				TitleFontID = Min(PloreBakedFont_Count-1, Context->Font->CurrentFont+5);
				TitleRect.P.Y += 4.0f;
			}

			vimgui_label_alignment Alignment = 0;
			if (Widget->Title.Alignment) Alignment = Widget->Title.Alignment;
			else if (Widget->Alignment) Alignment = Widget->Alignment;

			//
			// @Cleanup
			//
			PushRenderText(Context->RenderList,
						   (vimgui_render_text_desc) {
						       .Rect = TitleRect,
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

		if (Widget->Body.Text) {
			vimgui_label_alignment Alignment = 0;
			if (Widget->Body.Alignment) Alignment = Widget->Body.Alignment;
			else if (Widget->Alignment) Alignment = Widget->Alignment;
			PushRenderText(Context->RenderList,
						   (vimgui_render_text_desc) {
							   .Rect = Widget->Rect,
							   .Text = {
								   .Text = Widget->Body.Text,
								   .Alignment = Alignment,
								   .Colour = Widget->Body.Colour,
								   .Pad = Widget->Body.Pad,
								   .ColourFlags = Widget->Title.ColourFlags,
							   },
							   .FontID = SmallerFont,
						   });
		}

		if (Widget->Secondary.Text) {
			// NOTE(Evan): Clip the secondary text against the title, only if the title is left aligned.
			if (Widget->Title.Text && Widget->Title.Alignment == VimguiLabelAlignment_Left) {
				u64 TitleLength = StringLength(Widget->Title.Text)+1;

				rectangle P = Window->Rect;
				P.Span.W = P.Span.W - TitleLength*FontWidth;
				P.P.X += TitleLength*FontWidth;

				rectangle S = {
					.P = {
						.X = P.P.X,
						.Y = dY - (P.P.Y + P.Span.H)
					},
					.Span = P.Span,
				};

				PushRenderScissor(Context->RenderList, (vimgui_render_scissor_desc) {
									  .Rect = S,
								  });
			}

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


		if (Widget->Type == WidgetType_TextBox) {
			rectangle TextPad = Widget->Rect;

			f32 xPadFactor = 16.0f;
			TextPad.P.X += xPadFactor/2.0f;
			TextPad.Span.W -= xPadFactor;

			f32 yPadFactor = 4.0f;
			TextPad.P.Y += yPadFactor/2.0f;
			TextPad.Span.H -= yPadFactor;



			char *Lines = Widget->Text;
			if (Lines) {
				vimgui_widget_state *WidgetState = GetOrCreateWidgetState(Context, Widget->ID);
				u64 StartLine = WidgetState->TextBoxScroll;

				// NOTE(Evan): We can scroll "forward" through the file infinitely, but we never allow toroidal behaviour.
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
											   TextPad.P.X,
											   TextPad.P.Y + RenderedLines++*FontHeight,
										   },
										   .Span = TextPad.Span,
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
	Context->HotWidgetID = 0;

}

internal vimgui_window *
GetLayoutWindow(plore_vimgui_context *Context) {
	window_lookup *Lookup = MapGet(Context->Windows, &Context->ThisFrame.WindowWeAreLayingOut);

	return(MapIsDefault(Context->Windows, Lookup) ? 0 : &Lookup->V);
}

internal vimgui_window *
GetWindow(plore_vimgui_context *Context, u64 ID) {
	window_lookup *Lookup = MapGet(Context->Windows, &ID);

	return(MapIsDefault(Context->Windows, Lookup) ? 0 : &Lookup->V);
}

internal vimgui_widget *
GetWidget(plore_vimgui_context *Context, u64 ID) {
	widget_lookup *Lookup = MapGet(Context->Widgets, &ID);
	return(MapIsDefault(Context->Windows, Lookup) ? 0 : &Lookup->V);
}

internal vimgui_widget_state *
GetOrCreateWidgetState(plore_vimgui_context *Context, u64 ID) {
	widget_state_lookup *Lookup = MapGet(Context->WidgetState, &ID);

	if (MapIsDefault(Context->WidgetState, Lookup)) {
		if (MapFull(Context->WidgetState)) {
			Assert(false);
			//u64 *FirstID = MapIter(&Context->WidgetState).Key;
			//MapRemove(Context->WidgetState, FirstID);
		}
		Lookup = MapInsert(Context->WidgetState, &ID, &ClearStruct(vimgui_widget_state)).Value;
	}

	return(&Lookup->V);
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
	vimgui_label_desc Body;
	u64 ID;
	rectangle Rect;
	v2 Pad;
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

	// NOTE(Evan): Default args.

	if (Desc.FillWidth) {
		f32 FontHeight = GetCurrentFontHeight(Context->Font);
		f32 TitlePad = FontHeight+8.0f;
		f32 ButtonStartY = Window->Rect.P.Y + TitlePad;
		f32 RowHeight = Desc.Rect.Span.H;


		Desc.Rect.Span = (v2) {
			.W = Window->Rect.Span.X-8.0f,
			.H = RowHeight - Desc.Pad.Y,
		};

		Desc.Rect.P = (v2) {
			.X = Window->Rect.P.X+4.0f,
			.Y = ButtonStartY + (Window->HeightTotal - Window->HeightLeft),
		};


	} else {
		Desc.Rect.P.X += Window->Rect.P.X;
		Desc.Rect.P.Y += Window->Rect.P.Y;
	}

	if (Desc.Rect.Span.H > Window->HeightLeft) return(false);
	Window->HeightLeft -= (Desc.Rect.Span.H + Desc.Pad.Y);

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
				   .Body = Desc.Body,
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
		if (!MapFull(Context->Windows)) {
			MaybeWindow = MapInsert(Context->Windows, &MyID, &ClearStruct(vimgui_window)).Value;
			MaybeWindow->ID = MyID;
			MaybeWindow->Title = Desc.Title;
			MaybeWindow->NeverFocus = Desc.NeverFocus;
		}
	}

	if (MaybeWindow) {
		Assert(Context->ThisFrame.ParentStackCount < ArrayCount(Context->ThisFrame.ParentStack));
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
		MaybeWindow->HeightTotal = Desc.Rect.Span.H;
		MaybeWindow->HeightLeft = Desc.Rect.Span.H;

		f32 FontHeight = GetCurrentFontHeight(Context->Font);
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

internal void
WindowEnd(plore_vimgui_context *Context) {
	if (Context->ThisFrame.ParentStackCount) {
		u64 Window = Context->ThisFrame.ParentStack[--Context->ThisFrame.ParentStackCount-1];
		Context->ThisFrame.WindowWeAreLayingOut = Window;
	}
}

internal void
PushWidget(plore_vimgui_context *Context, vimgui_window *Parent, vimgui_widget Widget) {
	Assert(!MapFull(Context->Widgets));
	MapInsert(Context->Widgets, &Widget.ID, &Widget);
}

internal void
PushRenderText(plore_render_list *RenderList, vimgui_render_text_desc Desc) {
	Assert(RenderList && RenderList->CommandCount < ArrayCount(RenderList->Commands));

	f32 FontHeight = GetFontHeight(RenderList->Font, Desc.FontID);
	f32 FontWidth = GetFontWidth(RenderList->Font, Desc.FontID);

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
