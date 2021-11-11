internal void
VimguiBegin(plore_vimgui_context *Context, keyboard_and_mouse Input) {
	Assert(!Context->GUIPassActive);
	Context->GUIPassActive = true;
	
	Context->InputThisFrame = Input;
	Context->RenderList = (plore_render_list) {0};
}

internal void
VimguiEnd(plore_vimgui_context *Context) {
	Assert(Context->GUIPassActive);
	Context->GUIPassActive = false;
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
	if (IsWithinRectangleInclusive(Context->InputThisFrame.MouseP, Context->ActiveWindow->Rect)) {
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
		}
	}
	
	if (MaybeWindow) {
		if (IsWithinRectangleInclusive(Context->InputThisFrame.MouseP, MaybeWindow->Rect)) {
			if (Context->InputThisFrame.MouseLeftIsPressed) {
				PrintLine("PRESSED. ACTIVE.");
				Context->ActiveWindow = MaybeWindow;
				Colour.RGB = MultiplyVec3f(Colour.RGB, 1.20f);
			} else {
				PrintLine("NOT PRESSED. NOT ACTIVE.");
			}
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

