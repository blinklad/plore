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
PushRenderQuad(plore_render_list *RenderList, v2 P, v2 Span, v4 Colour) {
	Assert(RenderList && RenderList->QuadCount < ArrayCount(RenderList->Quads));
	
	RenderList->Quads[RenderList->QuadCount++] = (render_quad) {
		.Span   = Span,
		.P      = P,
		.Colour = Colour,
	};
}

internal b64
WindowTitled(plore_vimgui_context *Context, char *Title, v2 P, v2 Span, v4 Colour) {
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
		MaybeWindow->P = P;
		MaybeWindow->Span = Span;
		MaybeWindow->Colour = Colour;
		
		PushRenderQuad(&Context->RenderList, P, Span, Colour);
		
		PushRenderText(&Context->RenderList, 
					   (v2) { 
						   .X = MaybeWindow->P.X + MaybeWindow->Span.W/2.0f,
						   .Y = MaybeWindow->P.Y + MaybeWindow->Span.H,
					   },
					   WHITE_V4,
					   Title, 
					   true);
		
	}
	
	return(!!MaybeWindow);
	
}
