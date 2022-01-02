#include <stdio.h>
#include <stdlib.h>

#include "plore_common.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

u8 FontBuffer[1<<22];

#define GLYPH_COUNT 96

// NOTE(Evan): This must be invoked from "build/"
int
main(int ArgCount, char **Args) {
	char *OutputFile = "../code/generated/plore_baked_font.h";
	char *Font       = "../data/fonts/Inconsolata-Light.ttf";
	
	FILE *Output = fopen(OutputFile, "w");
	
	if (!Output) {
		printf("Could not open file %s", OutputFile); 
		return -1;
	}
	
	f32 Heights[] = {
		24.0f, 25.0f, 26.0f, 27.0f, 28.0f, 29.0f, 30.0f, 32.0f, 33.0f, 35.0f, 37.0f, 39.0f, 41.0f, 42.0f, 44.0f,
	};
	u8 *Bitmaps[ArrayCount(Heights)] = {0};
	stbtt_bakedchar BakedData[ArrayCount(Heights)][GLYPH_COUNT] = {0};
	
	
	FILE *FontFile = fopen(Font, "r");
	
	fseek(FontFile, 0, SEEK_END);
	size_t FontFileSize = ftell(FontFile);
	fseek(FontFile, 0, SEEK_SET);
	
	size_t BytesRead = fread(FontBuffer, 1, FontFileSize, FontFile);
	fclose(FontFile);
	
	ForArray(H, Heights) {
		Bitmaps[H] = calloc(512*512, 1);
		stbtt_BakeFontBitmap(FontBuffer, 0, Heights[H], Bitmaps[H], 512, 512, 32, GLYPH_COUNT, BakedData[H]); // No guarantee this fits!
	}
	
	
	
	fprintf(Output, "// Generated baked font\n");
	fprintf(Output, "#define PloreBakedFont_Count %d\n", (int)ArrayCount(Heights));
	
	fprintf(Output, "typedef struct plore_baked_font_bitmap {\n");
	fprintf(Output, "    f32 Height;\n");
	fprintf(Output, "    u8 Bitmap[512*512];\n");
    fprintf(Output, "} plore_baked_font_bitmap;\n");
	
	fprintf(Output, "typedef struct plore_baked_font_data {\n");
	fprintf(Output, "    f32 Height;\n");
	fprintf(Output, "    stbtt_bakedchar Data[%d];\n", GLYPH_COUNT);
    fprintf(Output, "} plore_baked_font_data;\n");
	
	fprintf(Output, "typedef struct plore_font {\n");
	fprintf(Output, "    plore_baked_font_data   *Data[PloreBakedFont_Count];   \n");
	fprintf(Output, "    plore_baked_font_bitmap *Bitmaps[PloreBakedFont_Count];\n");
	fprintf(Output, "    platform_texture_handle Handles[PloreBakedFont_Count]; \n");
	fprintf(Output, "    u64 CurrentFont;\n");
	fprintf(Output, "} plore_font;\n\n");
	
	fprintf(Output, "#ifdef PLORE_BAKED_FONT_IMPLEMENTATION\n");
	fprintf(Output, "plore_baked_font_bitmap BakedFontBitmaps[] = {\n");
	ForArray(H, Heights) {
		fprintf(Output, "\t[%d] = { .Height = %f, .Bitmap = {", (int)H, Heights[H]);
		
		for (u64 B = 0; B < 512*512; B++) {
			if ((B % 32) == 0) fprintf(Output, "\n\t");
			fprintf(Output, " 0x%02x,", Bitmaps[H][B]);
		}
		
		fprintf(Output, "\n}},");
	}
	fprintf(Output, "};\n");
	
	
	fprintf(Output, "plore_baked_font_data BakedFontData[] = {\n");
	ForArray(H, Heights) {
		fprintf(Output, "\t[%d] = { .Height = %f,\n\t\t.Data = {\n", (int)H, Heights[H]);
		
		for (u64 D = 0; D < GLYPH_COUNT; D++) {
			stbtt_bakedchar *Data = BakedData[H] + D;
			fprintf(Output, "\t\t[%d] = {\n\t\t\t", (int)D);
			fprintf(Output, ".x0 = %d, .y0 = %d, .x1 = %d, .y1 = %d,\n\t\t\t", Data->x0, Data->y0, Data->x1, Data->y1);
			fprintf(Output, ".xoff = %f, .yoff = %f, .xadvance = %f\n\t\t},\n", Data->xoff, Data->yoff, Data->xadvance);
		}
		fprintf(Output, "}},\n");
	}
	fprintf(Output, "};");
	
	fprintf(Output, "\n#endif // PLORE_BAKED_FONT_IMPLEMENTATION\n");
	fflush(Output);
	fclose(Output);
	
	return 0;
}