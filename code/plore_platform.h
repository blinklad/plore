#ifndef PLORE_PLATFORM
#define PLORE_PLATFORM

typedef enum plore_file_node { 
	PloreFileNode_File, 
	PloreFileNode_Directory 
} plore_file_node;
#define PLORE_MAX_PATH 256

typedef struct plore_file {
	char AbsolutePath[PLORE_MAX_PATH];
	char *FileNameInPath;
	plore_file_node Type;
} plore_file;


typedef struct platform_readable_file {
    void *Opaque;
    uint64 FileSize;
    bool32 OpenedSuccessfully;
} platform_readable_file;


#define PLATFORM_DEBUG_OPEN_FILE(name) platform_readable_file name(plore_file *File)
typedef PLATFORM_DEBUG_OPEN_FILE(platform_debug_open_file);


typedef struct platform_read_file_result {
    void *Buffer;
    uint64 BytesRead;
    bool32 ReadSuccessfully;
} platform_read_file_result;
#define PLATFORM_DEBUG_READ_ENTIRE_FILE(name) platform_read_file_result name(platform_readable_file *File, void *Buffer, uint64 BufferSize)
typedef PLATFORM_DEBUG_READ_ENTIRE_FILE(platform_debug_read_entire_file);

#define PLATFORM_DEBUG_PRINT_LINE(name) void name(const char *Format, ...)
typedef PLATFORM_DEBUG_PRINT_LINE(platform_debug_print_line);

#define PLATFORM_DEBUG_PRINT(name) void name(const char *Format, ...)
typedef PLATFORM_DEBUG_PRINT(platform_debug_print);


typedef struct platform_texture_handle {
	u32 Handle;
	f32 Width;
	f32 Height;
} platform_texture_handle;

#define PLATFORM_CREATE_TEXTURE_HANDLE(name) platform_texture_handle name(void *Pixels, u64 Width, u64 Height)
typedef PLATFORM_CREATE_TEXTURE_HANDLE(platform_create_texture_handle);

#define PLATFORM_DESTROY_TEXTURE_HANDLE(name) void name(platform_texture_handle Texture)
typedef PLATFORM_DESTROY_TEXTURE_HANDLE(platform_destroy_texture_handle);

#define PLATFORM_SHOW_CURSOR(name) void name(b64 Show)
typedef PLATFORM_SHOW_CURSOR(platform_show_cursor);

#define PLATFORM_GET_CURRENT_DIRECTORY(name) void name(char *Buffer, u64 Size)
typedef PLATFORM_GET_CURRENT_DIRECTORY(platform_get_current_directory);

#define PLATFORM_POP_PATH_NODE(name) void name(char *Buffer, u64 Size);
typedef PLATFORM_POP_PATH_NODE(platform_pop_path_node);

typedef struct directory_entry_result {
	char *DirectoryName;
	plore_file *Buffer;
	u64 Size;
	u64 Count;
	
	u64 IgnoredCount;
	b64 Succeeded;
} directory_entry_result;
#define PLATFORM_GET_DIRECTORY_ENTRIES(name) directory_entry_result name(char *DirectoryName, plore_file *Buffer, u64 Size);
typedef PLATFORM_GET_DIRECTORY_ENTRIES(platform_get_directory_entries);

// NOTE(Evan): Platform API
typedef struct platform_api {
    u64 WindowWidth;
    u64 WindowHeight;

	platform_create_texture_handle  *CreateTextureHandle;
	platform_destroy_texture_handle *DestroyTextureHandle;
	
	platform_show_cursor            *ShowCursor;
	
    platform_debug_open_file        *DebugOpenFile;
    platform_debug_read_entire_file *DebugReadEntireFile;
    platform_debug_print_line       *DebugPrintLine;
    platform_debug_print            *DebugPrint;
	
	platform_get_directory_entries  *GetDirectoryEntries;
	platform_get_current_directory  *GetCurrentDirectory;
	platform_pop_path_node          *PopPathNode;
} platform_api;

#endif