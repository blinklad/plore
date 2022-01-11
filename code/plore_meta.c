#include <stdio.h>
#include <stdlib.h>

#include "plore_common.h"


#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

u8 FontBuffer[1<<22];

#define FONT_MAX_WIDTH 256
#define FONT_MAX_HEIGHT 256

#define GLYPH_COUNT 96


// NOTE(Evan): Registry reading bits.
#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#define MAX_KEY_LENGTH 128
#define MAX_VALUE_NAME 128

// NOTE(Evan): This is not used by the registry functions, just the hash table.
#define MAX_EXTENSION_COUNT 2048

void QueryKey(HKEY hKey);

#include "plore_string.h"
#include "plore_memory.h"
#include "plore_memory.c"
#define PLORE_MAP_IMPLEMENTATION
#include "plore_map.h"

#define META_ARENA_SIZE Megabytes(512)

plore_map ExtensionDefaultMap;
memory_arena MetaArena;
typedef struct extension_default_key {
	char Extension[MAX_KEY_LENGTH];
	char Handler[MAX_KEY_LENGTH];
	HKEY hKey;
} extension_default_key;

typedef struct extension_default_value {
	char Shell[MAX_VALUE_NAME];
	HKEY hKey;
} extension_default_value;

// NOTE(Evan): This must be invoked from "build/"
int
main(int ArgCount, char **Args) {
	MetaArena = CreateMemoryArena(calloc(META_ARENA_SIZE, 1), META_ARENA_SIZE);
	ExtensionDefaultMap = MapInit(&MetaArena, extension_default_key, extension_default_value, MAX_EXTENSION_COUNT);
	
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
	
	size_t BytesRead = fread(FontBuffer, 1, FontFileSize, FontFile);
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
	
	
	HKEY hTestKey;
	
	if( RegOpenKeyExA( HKEY_CLASSES_ROOT,
					 0,
					 0,
					 KEY_READ,
					 &hTestKey) == ERROR_SUCCESS
	   )
	{
		QueryKey(hTestKey);
	}
	
	RegCloseKey(hTestKey);
	
	char *WindowsHandlerOutputFilePath = "../code/generated/win32_plore_handler.h";
	FILE *WindowsHandlerOutputFile = fopen(WindowsHandlerOutputFilePath, "w");
	
	if (!WindowsHandlerOutputFile) {
		printf("Could not open file %s", WindowsHandlerOutputFilePath); 
		return -1;
	}
	
	FILE *F = WindowsHandlerOutputFile;
	fprintf(F, "//\n");
	fprintf(F, "// win32_plore_handler.h\n");
	fprintf(F, "// Automatically generated Windows file handlers based on registry entries.\n");
	fprintf(F, "// Handler count: %d\n", (int)ExtensionDefaultMap.Count);
	fprintf(F, "//\n\n");
	fprintf(F, "#define PLORE_HANDLER_MAX %d\n", (int)ExtensionDefaultMap.Count);
	
	temp_string Enums = TempString(&MetaArena, Megabytes(4));
	temp_string LongStrings = TempString(&MetaArena, Megabytes(4));
	temp_string ShortStrings = TempString(&MetaArena, Megabytes(4));
	temp_string Handlers = TempString(&MetaArena, Megabytes(4));
	
	TempCat(Enums,        "typedef enum plore_file_extension {\n");
	TempCat(LongStrings,  "char *PloreFileExtensionLongStrings[] = {\n");
	TempCat(ShortStrings, "char *PloreFileExtensionShortStrings[] = {\n");
	TempCat(Handlers,     "plore_handler PloreFileExtensionHandlers[PLORE_FILE_EXTENSION_HANDLER_MAX] = {\n");
	
	#if 1
	for (plore_map_iterator It = MapIter(&ExtensionDefaultMap);
		 !It.Finished;
		 It = MapIterNext(&ExtensionDefaultMap, It)) {
		extension_default_value *Value = It.Value;
		extension_default_key *Key = It.Key;
		
		TempCat(Enums, "\tPloreFileExtension_%s, \n", Key->Extension+1);
		TempCat(ShortStrings, "\t\"%s\",\n", Key->Extension+1);
		TempCat(LongStrings, "\t\"%s\",\n", Key->Handler);
		TempCat(Handlers, "\t{ .Shell = %s, .Name = \"%s\" },\n", Value->Shell, Key->Handler);
	}
	
	TempCat(Enums, "\tPloreFileExtension_Count, \n");
	TempCat(Enums, "\t_PloreFileExtension_ForceU64 = 0xffffffffull, \n");
	TempCat(Enums, "} plore_file_extension;");
	
	TempCat(ShortStrings, "}\n");
	TempCat(LongStrings,  "}\n");
	TempCat(Handlers,     "}\n");
	
	fprintf(F, "%s\n%s\n%s\n%s\n\n", Enums.Buffer, ShortStrings.Buffer, LongStrings.Buffer, Handlers.Buffer);
	printf("%s\n%s\n%s\n%s\n\n", Enums.Buffer, ShortStrings.Buffer, LongStrings.Buffer, Handlers.Buffer);
	
	#endif
	fflush(WindowsHandlerOutputFile);
	fclose(WindowsHandlerOutputFile);
	
	return 0;
}

void QueryKey(HKEY hKey) 
{ 
    DWORD    cbName;                   // size of name string 
    char     achClass[MAX_PATH] = TEXT("");  // buffer for class name 
    DWORD    cchClassName = MAX_PATH;  // size of class string 
    DWORD    cSubKeys=0;               // number of subkeys 
    DWORD    cbMaxSubKey;              // longest subkey size 
    DWORD    cchMaxClass;              // longest class string 
    DWORD    cValues;              // number of values for key 
    DWORD    cchMaxValue;          // longest value name 
    DWORD    cbMaxValueData;       // longest value data 
    DWORD    cbSecurityDescriptor; // size of security descriptor 
    FILETIME ftLastWriteTime;      // last write time 
	
    DWORD i, retCode; 
	
    // Get the class name and the value count. 
    retCode = RegQueryInfoKeyA(
							  hKey,                    // key handle 
							  achClass,                // buffer for class name 
							  &cchClassName,           // size of class string 
							  NULL,                    // reserved 
							  &cSubKeys,               // number of subkeys 
							  &cbMaxSubKey,            // longest subkey size 
							  &cchMaxClass,            // longest class string 
							  &cValues,                // number of values for this key 
							  &cchMaxValue,            // longest value name 
							  &cbMaxValueData,         // longest value data 
							  &cbSecurityDescriptor,   // security descriptor 
							  &ftLastWriteTime);       // last write time 
	
    // Enumerate the subkeys, until RegEnumKeyEx fails.
    
    if (cSubKeys)
    {
        printf( "\nNumber of subkeys: %d\n", cSubKeys);
		
        for (i=0; i<cSubKeys; i++) 
        { 
            cbName = MAX_KEY_LENGTH;
			extension_default_key ChildKey = {0};
            retCode = RegEnumKeyExA(hKey, i,
								   ChildKey.Extension,
								   &cbName, 
								   NULL, 
								   NULL, 
								   NULL, 
								   &ftLastWriteTime); 
			
            if (retCode == ERROR_SUCCESS && ChildKey.Extension[0] == '.') 
            {
//                _tprintf(TEXT("(%d) %s\n"), i+1, ChildKey.Extension);
				
				
				if (RegOpenKeyExA(HKEY_CLASSES_ROOT,
								 ChildKey.Extension,
								 0,
								 KEY_READ,
								 &ChildKey.hKey) == ERROR_SUCCESS) {
					
					
					#if 1
					
					DWORD ChildBufferSize = MAX_KEY_LENGTH;
					LSTATUS Result =  RegGetValueA(
										 ChildKey.hKey,
										 0,
										 0,
										 RRF_RT_ANY,
										 0,
										 ChildKey.Handler,
										 &ChildBufferSize
										 );						
					
					//_tprintf(TEXT("\t%s"), ChildKey.Handler); 
					extension_default_value ChildValue = {0};
					
					if (Result == ERROR_SUCCESS) { 
						
						if (RegOpenKeyExA(HKEY_CLASSES_ROOT,
										  ChildKey.Handler,
										  0,
										  KEY_READ,
										  &ChildValue.hKey) == ERROR_SUCCESS) {
							
							if (RegOpenKeyExA(ChildValue.hKey, "shell\\open\\command", 0, KEY_READ, &ChildValue.hKey) == ERROR_SUCCESS) {
								DWORD ChildBufferSize = MAX_VALUE_NAME;
								LSTATUS Result =  RegGetValueA(
															   ChildValue.hKey,
															   0,
															   0,
															   RRF_RT_ANY,
															   0,
															   ChildValue.Shell,
															   &ChildBufferSize
															   );					
								if (Result == ERROR_SUCCESS) {
									//_tprintf(TEXT(", %s\n"), ChildValue.Buffer); 
									MapInsert(&ExtensionDefaultMap, &ChildKey, &ChildValue);
								}
							}
						}
				    }
					#endif
				}
            }
        }
    } 
	
	
	for (plore_map_iterator It = MapIter(&ExtensionDefaultMap);
		 !It.Finished;
		 It = MapIterNext(&ExtensionDefaultMap, It)) {
		
		extension_default_value *Value = It.Value;
		extension_default_key *Key = It.Key;
		
		char *C = Value->Shell;
		u64 CharCount = 0;
		u64 QuoteCount = 0;
		while (*C) {
			if (*C == '"') QuoteCount++;
			
			if (*C == '\\' && C[1] != '\\') {
				char Temp[MAX_VALUE_NAME] = {0};
				StringCopy(C+1, Temp, ArrayCount(Temp));
				C[1] = '\\';
				StringCopy(Temp, C+2, MAX_VALUE_NAME);
				C++;
			}
			
			if (*C == '%' || *C == '-') {
				*C = '\0';
				if ((QuoteCount % 2) != 0) {
					while (*C-- != '"');
					*C = '\0';
				}
				break;
			}
			
			C++;
			CharCount++;
			if (CharCount >= 128) break;
		}
		
		C = Key->Extension;
		while (*C) {
			if (*C == '-') *C = '_';
			C++;
		}
	}
	
	for (plore_map_iterator It = MapIter(&ExtensionDefaultMap);
		 !It.Finished;
		 It = MapIterNext(&ExtensionDefaultMap, It)) {
		extension_default_value *Value = It.Value;
		extension_default_key *Key = It.Key;
		//printf("%s (%s)\n\t- %s\n", Key->Extension+1, Key->Handler, Value->Shell);
	}
	
		 
}