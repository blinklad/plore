#include "plore_common.h"


#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

u8 FontBuffer[1<<22];

#define FONT_MAX_WIDTH 256
#define FONT_MAX_HEIGHT 256

#define GLYPH_COUNT 96


#if defined(PLORE_WINDOWS)
#include "win32_meta.c"
#elif defined(PLORE_LINUX)
#include "linux_meta.c"
#else
#error Unsupported platform.
#endif

#include <stdlib.h>
#include <stdio.h>

#include "plore_string.h"
#include "plore_memory.h"
#include "plore_memory.c"
#define PLORE_MAP_IMPLEMENTATION
#include "plore_map.h"

memory_arena MetaArena;
#define META_ARENA_SIZE Megabytes(512)


// NOTE(Evan): This must be invoked from "build/"
int
main(int ArgCount, char **Args) {
	MetaArena = CreateMemoryArena(calloc(META_ARENA_SIZE, 1), META_ARENA_SIZE);

	char *FontOutputFilePath = "../code/generated/plore_baked_font.h";
	char *Font       = "../data/fonts/Inconsolata-Light.ttf";

	FILE *FontOutputFile = fopen(FontOutputFilePath, "w");

	if (!FontOutputFile) {
		printf("Could not open file %s", FontOutputFilePath);
		return -1;
	}

	f32 Heights[] = {
		24.0f, 25.0f, 26.0f, 27.0f, 28.0f,
		29.0f, 30.0f, 32.0f, 33.0f, 35.0f,
		37.0f, 39.0f, 41.0f, 42.0f, 44.0f,
	};
	u8 *Bitmaps[ArrayCount(Heights)] = {0};
	stbtt_bakedchar BakedData[ArrayCount(Heights)][GLYPH_COUNT] = {0};


	FILE *FontFile = fopen(Font, "r");

	fseek(FontFile, 0, SEEK_END);
	size_t FontFileSize = ftell(FontFile);
	fseek(FontFile, 0, SEEK_SET);

	fread(FontBuffer, 1, FontFileSize, FontFile);
	fclose(FontFile);

	ForArray(H, Heights) {
		Bitmaps[H] = calloc(FONT_MAX_WIDTH*FONT_MAX_HEIGHT, 1);
		stbtt_BakeFontBitmap(FontBuffer, 0, Heights[H], Bitmaps[H], FONT_MAX_WIDTH, FONT_MAX_HEIGHT, 32, GLYPH_COUNT, BakedData[H]); // No guarantee this fits!
	}



	fprintf(FontOutputFile, "//\n");
	fprintf(FontOutputFile, "// plore_baked_font.h\n");
	fprintf(FontOutputFile, "// Automatically generated, baked font bitmap and definitions\n");
	fprintf(FontOutputFile, "// Font: `%s`\n", Font);
	fprintf(FontOutputFile, "//\n");
	fprintf(FontOutputFile, "#define PloreBakedFont_Count %d\n", (int)ArrayCount(Heights));

	fprintf(FontOutputFile, "typedef struct plore_baked_font_bitmap {\n");
	fprintf(FontOutputFile, "    f32 Height;\n");
	fprintf(FontOutputFile, "    u64 BitmapWidth;\n");
	fprintf(FontOutputFile, "    u64 BitmapHeight;\n");
	fprintf(FontOutputFile, "    u8 Bitmap[%d*%d];\n", FONT_MAX_WIDTH, FONT_MAX_HEIGHT);
    fprintf(FontOutputFile, "} plore_baked_font_bitmap;\n");

	fprintf(FontOutputFile, "typedef struct plore_baked_font_data {\n");
	fprintf(FontOutputFile, "    f32 Height;\n");
	fprintf(FontOutputFile, "    stbtt_bakedchar Data[%d];\n", GLYPH_COUNT);
    fprintf(FontOutputFile, "} plore_baked_font_data;\n");

	fprintf(FontOutputFile, "typedef struct plore_font {\n");
	fprintf(FontOutputFile, "    plore_baked_font_data   *Data[PloreBakedFont_Count];   \n");
	fprintf(FontOutputFile, "    plore_baked_font_bitmap *Bitmaps[PloreBakedFont_Count];\n");
	fprintf(FontOutputFile, "    platform_texture_handle Handles[PloreBakedFont_Count]; \n");
	fprintf(FontOutputFile, "    u64 CurrentFont;\n");
	fprintf(FontOutputFile, "} plore_font;\n\n");

	fprintf(FontOutputFile, "#ifdef PLORE_BAKED_FONT_IMPLEMENTATION\n");
	fprintf(FontOutputFile, "plore_baked_font_bitmap BakedFontBitmaps[] = {\n");
	ForArray(H, Heights) {
		fprintf(FontOutputFile, "\t[%d] = { .Height = %f, .BitmapWidth = %d, .BitmapHeight = %d, .Bitmap = {", (int)H, Heights[H], FONT_MAX_WIDTH, FONT_MAX_HEIGHT);

		for (u64 B = 0; B < FONT_MAX_WIDTH*FONT_MAX_HEIGHT; B++) {
			if ((B % 32) == 0) fprintf(FontOutputFile, "\n\t");
			fprintf(FontOutputFile, " 0x%02x,", Bitmaps[H][B]);
		}

		fprintf(FontOutputFile, "\n}},");
	}
	fprintf(FontOutputFile, "};\n");


	fprintf(FontOutputFile, "plore_baked_font_data BakedFontData[] = {\n");
	ForArray(H, Heights) {
		fprintf(FontOutputFile, "\t[%d] = { .Height = %f,\n\t\t.Data = {\n", (int)H, Heights[H]);

		for (u64 D = 0; D < GLYPH_COUNT; D++) {
			stbtt_bakedchar *Data = BakedData[H] + D;
			fprintf(FontOutputFile, "\t\t[%d] = {\n\t\t\t", (int)D);
			fprintf(FontOutputFile, ".x0 = %d, .y0 = %d, .x1 = %d, .y1 = %d,\n\t\t\t", Data->x0, Data->y0, Data->x1, Data->y1);
			fprintf(FontOutputFile, ".xoff = %f, .yoff = %f, .xadvance = %f\n\t\t},\n", Data->xoff, Data->yoff, Data->xadvance);
		}
		fprintf(FontOutputFile, "}},\n");
	}
	fprintf(FontOutputFile, "};");

	fprintf(FontOutputFile, "\n#endif // PLORE_BAKED_FONT_IMPLEMENTATION\n");
	fflush(FontOutputFile);
	fclose(FontOutputFile);


	PlatformDoMetaprogram();
}
