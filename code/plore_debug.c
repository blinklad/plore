global plore_state *GlobalState;

internal void
DebugInit(plore_state *State) {
	GlobalState = State;
}

internal void
DrawText(char *Format, ...);

internal void
FlushText(void);

typedef struct debug_text {
	f32 T;
	char Buffer[128];
} debug_text;

global debug_text DebugText[8] = {0};
global u64 DebugTextCount = 0;

internal void
DrawText(char *Format, ...) {
	if (DebugTextCount >= ArrayCount(DebugText)-1) return;
	
	// NOTE(Evan): Check if a duplicate string already exists.
	// CLEANUP(Evan): __LINE__ might work?
	char Buffer[128];
	va_list Args;
	va_start(Args, Format);
	stbsp_vsnprintf(Buffer, ArrayCount(Buffer), Format, Args);
	va_end(Args);
	
	for (u64 T = 0; T < DebugTextCount; T++) {
		debug_text *Text = DebugText + T;
		if (StringsAreEqual(Text->Buffer, Buffer)) return;
	}
	
	
	// Text is unique.
	debug_text *Text = DebugText + DebugTextCount++;
	*Text = (debug_text) {0};
	MemoryCopy(Buffer, Text->Buffer, sizeof(Text->Buffer));
}

internal void
FlushText(void) {
	u64 W = GlobalState->VimguiContext->WindowDimensions.W;
	u64 H = GlobalState->VimguiContext->WindowDimensions.H;
	f32 StartY = GlobalState->VimguiContext->WindowDimensions.H * 0.5f;
	for (u64 T = 0; T < DebugTextCount; T++) {
		debug_text *Text = DebugText + T;
		if (Text->T >= 1.2f) {
			DebugText[T] = DebugText[--DebugTextCount];
		} else {
			f32 Fade = 1.0f;
			if (Text->T > 0.6f) {
				Fade -= Lerp(0.0f, (Text->T - 0.6f) / 1.0f, 1.2f);
			}
			v4 Colour = WHITE_V4;
			Colour.A = Fade;
					
			Text->T += GlobalState->DT;
			
			PushRenderScissor(GlobalState->RenderList,
							  (vimgui_render_scissor_desc) {
								  .Rect = {
									  .P = V2(0, 0),
									  .Span = GlobalState->VimguiContext->WindowDimensions,
								  }
							  });
								  
			PushRenderText(GlobalState->RenderList,
						   (vimgui_render_text_desc) {
							   .Rect =  { 
								   .P = {
									   0,
									   StartY - T * 100,
								   },
								   .Span = { 
									   W, 100 
								   },
							   },
							   ._TextColour = V4(0, 0, 0, Fade),
							   .Text = { 
								   .Text = Text->Buffer,
								   .Alignment = VimguiLabelAlignment_Center, 
							   },
							   .FontID = PloreBakedFont_Count-1,
						   }
						   );
			
			PushRenderText(GlobalState->RenderList,
						   (vimgui_render_text_desc) {
						   .Rect = { 
							   .P = {
								   0,
								   StartY - T * 100,
							   },
							   .Span = { 
								   W, 100 
							   },
						   },
						   ._TextColour = Colour,
						   .Text = { 
							   .Text = Text->Buffer,
							   .Alignment = VimguiLabelAlignment_Center, 
						   },
							   .FontID = PloreBakedFont_Count-1,
						   });
		}
	}
}