// NOTE(Evan): Registry reading bits.
#include <windows.h>
#include <tchar.h>

#define MAX_KEY_LENGTH 128
#define MAX_VALUE_NAME 128

// NOTE(Evan): This is not used by the registry functions, just the hash table.
#define MAX_EXTENSION_COUNT 2048

plore_map ExtensionDefaultMap;
typedef struct extension_default_key {
	char Extension[MAX_KEY_LENGTH];
	char Handler[MAX_KEY_LENGTH];
	HKEY hKey;
} extension_default_key;

typedef struct extension_default_value {
	char Shell[MAX_VALUE_NAME];
	HKEY hKey;
} extension_default_value;
internal void
QueryKey(HKEY hKey);

internal void
PlatformDoMetaprogram() {
	ExtensionDefaultMap = MapInit(&MetaArena, extension_default_key, extension_default_value, MAX_EXTENSION_COUNT);
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

void QueryKey(HKEY hKey) {
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

