#ifndef PLORE_PLATFORM
#define PLORE_PLATFORM

#include "plore_file.h"

typedef struct platform_taskmaster platform_taskmaster;

typedef struct platform_readable_file {
    void *Opaque;
    u64 FileSize;
    b64 OpenedSuccessfully;
} platform_readable_file;

#define PLATFORM_DEBUG_OPEN_FILE(name) platform_readable_file name(char *Path)
typedef PLATFORM_DEBUG_OPEN_FILE(platform_debug_open_file);

#define PLATFORM_DEBUG_CLOSE_FILE(name) void name(platform_readable_file File)
typedef PLATFORM_DEBUG_CLOSE_FILE(platform_debug_close_file);

typedef struct platform_read_file_result {
    void *Buffer;
    u64 BytesRead;
    b64 ReadSuccessfully;
} platform_read_file_result;
#define PLATFORM_DEBUG_READ_ENTIRE_FILE(name) platform_read_file_result name(platform_readable_file File, void *Buffer, u64 BufferSize)
typedef PLATFORM_DEBUG_READ_ENTIRE_FILE(platform_debug_read_entire_file);

#define PLATFORM_DEBUG_READ_FILE_SIZE(name) platform_read_file_result name(platform_readable_file File, void *Buffer, u64 BufferSize)
typedef PLATFORM_DEBUG_READ_FILE_SIZE(platform_debug_read_file_size);

#define PLATFORM_CREATE_FILE(name) b64 name(char *Path, b64 OverwriteExisting)
typedef PLATFORM_CREATE_FILE(platform_create_file);

#define PLATFORM_CREATE_DIRECTORY(name) b64 name(char *Path)
typedef PLATFORM_CREATE_DIRECTORY(platform_create_directory);

typedef struct platform_delete_file_desc {
	b64 Recursive;
	b64 PermanentDelete;
} platform_delete_file_desc;

#define PLATFORM_DELETE_FILE(name) b64 name(char *Path, platform_delete_file_desc Desc)
typedef PLATFORM_DELETE_FILE(platform_delete_file);

#define PLATFORM_DEBUG_PRINT_LINE(name) void name(const char *Format, ...)
typedef PLATFORM_DEBUG_PRINT_LINE(platform_debug_print_line);

#define PLATFORM_DEBUG_PRINT(name) void name(const char *Format, ...)
typedef PLATFORM_DEBUG_PRINT(platform_debug_print);


//
// CLEANUP(Evan): Move implementation of renderer bits, texture implementation, etc, once we implement 3.x core profile.
//

typedef struct platform_texture_handle {
	u64 Opaque;
	f32 Width;
	f32 Height;
} platform_texture_handle;

typedef enum pixel_format {
	PixelFormat_RGBA8,
	PixelFormat_RGB8,
	PixelFormat_BGRA8,
	PixelFormat_ALPHA
} pixel_format;

typedef struct platform_texture_handle_desc {
	u8 *Pixels;
	u64 Width;
	u64 Height;
	pixel_format ProvidedPixelFormat;
	pixel_format TargetPixelFormat;
	enum { FilterMode_Nearest, FilterMode_Linear } FilterMode;
} platform_texture_handle_desc;

#define PLATFORM_CREATE_TEXTURE_HANDLE(name) platform_texture_handle name(platform_texture_handle_desc Desc)
typedef PLATFORM_CREATE_TEXTURE_HANDLE(platform_create_texture_handle);

#define PLATFORM_DESTROY_TEXTURE_HANDLE(name) void name(platform_texture_handle Texture)
typedef PLATFORM_DESTROY_TEXTURE_HANDLE(platform_destroy_texture_handle);

#define PLATFORM_SHOW_CURSOR(name) void name(b64 Show)
typedef PLATFORM_SHOW_CURSOR(platform_show_cursor);

#define PLATFORM_TOGGLE_FULLSCREEN(name) void name()
typedef PLATFORM_TOGGLE_FULLSCREEN(platform_toggle_fullscreen);

#define PLATFORM_GET_CURRENT_DIRECTORY_PATH(name) plore_path name()
typedef PLATFORM_GET_CURRENT_DIRECTORY_PATH(platform_get_current_directory_path);

#define PLATFORM_SET_CURRENT_DIRECTORY(name) b64 name(char *Path)
typedef PLATFORM_SET_CURRENT_DIRECTORY(platform_set_current_directory);


// TODO(Evan): plore_path_buffer/plore_path should be used throughout the API.
// We do not want the flexibility of arbitrarily-sized path buffers, degenerate paths are not worth supporting.
//
// However, the platform_debug_* functions can use arbitrarily null-terminated paths.
// That way, quick-and-dirty debugging with string literals works as expected.

typedef struct platform_path_pop_result {
	b64 DidRemoveSomething;
	char *AbsolutePath;
	char *FilePart;
} platform_path_pop_result;

#define PLATFORM_PATH_POP(name) platform_path_pop_result name(char *Buffer, u64 BufferSize, b64 AddTrailingSlash)
typedef PLATFORM_PATH_POP(platform_path_pop);

#define PLATFORM_PATH_IS_DIRECTORY(name) b64 name(char *Buffer)
typedef PLATFORM_PATH_IS_DIRECTORY(platform_path_is_directory);

#define PLATFORM_PATH_IS_TOP_LEVEL(name) b64 name(char *Buffer)
typedef PLATFORM_PATH_IS_TOP_LEVEL(platform_path_is_top_level);

#define PLATFORM_PATH_JOIN(name) b64 name(char *Buffer, char *First, char *Second)
typedef PLATFORM_PATH_JOIN(platform_path_join);

typedef struct directory_entry_result {
	char *Name;         // NOTE(Evan): Aliases parameter.
	plore_file *Buffer; // NOTE(Evan): Aliases parameter.
	u64 Size;
	u64 Count;

	u64 IgnoredCount;
	b64 Succeeded;
} directory_entry_result;

#define PLATFORM_GET_DIRECTORY_ENTRIES(name) directory_entry_result name(char *DirectoryName, plore_file *Buffer, u64 Size)
typedef PLATFORM_GET_DIRECTORY_ENTRIES(platform_get_directory_entries);

#define PLATFORM_DIRECTORY_SIZE_TASK_BEGIN(name) void name(platform_taskmaster *Taskmaster, plore_path *Path, plore_directory_query_state *State)
typedef PLATFORM_DIRECTORY_SIZE_TASK_BEGIN(platform_directory_size_task_begin);

#define PLATFORM_MOVE_FILE(name) b64 name(char *sAbsolutePath, char *dAbsolutePath)
typedef PLATFORM_MOVE_FILE(platform_move_file);

#define PLATFORM_RENAME_FILE(name) b64 name(char *sAbsolutePath, char *dAbsolutePath)
typedef PLATFORM_RENAME_FILE(platform_rename_file);

typedef struct platform_run_shell_desc {
	char *Command;
	char *Args;
	b64 QuoteArgs;
} platform_run_shell_desc;

#define PLATFORM_RUN_SHELL(name) b64 name(platform_run_shell_desc Desc)
typedef PLATFORM_RUN_SHELL(platform_run_shell);


// NOTE(Evan): Task bits.

typedef struct memory_arena memory_arena;
typedef struct platform_taskmaster platform_taskmaster;

typedef struct platform_task_info {
	u64 ThreadID;
	memory_arena *TaskArena;
} platform_task_info;

#define PLATFORM_TASK(name) void name(platform_task_info Info, void *Param)
typedef PLATFORM_TASK(task_procedure);

typedef struct platform_task {
	task_procedure *Procedure;
	void *Param;
	memory_arena Arena; // NOTE(Evan): platform_tasks and thread_contexts have arenas for task-creation-time and runtime respectively.
	b64 Running;
	char Pad[24]; // NOTE(Evan): Align to cache size assumably of 64 bytes. This could be determined more rigorously.
} platform_task;

typedef struct task_with_memory_handle {
	memory_arena *Arena;
	u64 ID;
} task_with_memory_handle;

#define PLATFORM_CREATE_TASK_WITH_MEMORY(name) task_with_memory_handle name(platform_taskmaster *Taskmaster)
typedef PLATFORM_CREATE_TASK_WITH_MEMORY(platform_create_task_with_memory);

#define PLATFORM_START_TASK_WITH_MEMORY(name) void name(platform_taskmaster *Taskmaster, task_with_memory_handle Handle, task_procedure *Procedure, void *Param)
typedef PLATFORM_START_TASK_WITH_MEMORY(platform_start_task_with_memory);

#define PLATFORM_PUSH_TASK(name) void name(platform_taskmaster *Taskmaster, task_procedure *Procedure, void *Param)
typedef PLATFORM_PUSH_TASK(platform_push_task);


// NOTE(Evan): Returns whether to execute debug trap.
#define PLATFORM_DEBUG_ASSERT_HANDLER(name) b64 name(char *Message)
typedef PLATFORM_DEBUG_ASSERT_HANDLER(platform_debug_assert_handler);

// NOTE(Evan): Platform API
typedef struct platform_api {
	platform_taskmaster *Taskmaster;

	union {
		v2 WindowDimensions;
		struct {
			f32 WindowWidth;
			f32 WindowHeight;
		};
	};

	platform_debug_assert_handler        *DebugAssertHandler;
	platform_create_texture_handle       *CreateTextureHandle;
	platform_destroy_texture_handle      *DestroyTextureHandle;

	platform_show_cursor                 *ShowCursor;
	platform_toggle_fullscreen           *ToggleFullscreen;

    platform_debug_open_file             *DebugOpenFile;
    platform_debug_read_entire_file      *DebugReadEntireFile;
    platform_debug_read_file_size        *DebugReadFileSize;
	platform_debug_close_file            *DebugCloseFile;
    platform_debug_print_line            *DebugPrintLine;
    platform_debug_print                 *DebugPrint;

	platform_directory_size_task_begin   *DirectorySizeTaskBegin;

	platform_get_directory_entries       *GetDirectoryEntries;
	platform_get_current_directory_path  *GetCurrentDirectoryPath;
	platform_set_current_directory       *SetCurrentDirectory;

	platform_path_pop                    *PathPop;
	platform_path_join                   *PathJoin;
	platform_path_is_directory           *PathIsDirectory;
	platform_path_is_top_level           *PathIsTopLevel;

	platform_create_file                 *CreateFile;
	platform_create_directory            *CreateDirectory;
	platform_move_file                   *MoveFile;
	platform_rename_file                 *RenameFile;
	platform_delete_file                 *DeleteFile;
	platform_run_shell                   *RunShell;

	platform_create_task_with_memory     *CreateTaskWithMemory;
	platform_start_task_with_memory      *StartTaskWithMemory;
	platform_push_task                   *PushTask;
} platform_api;

#endif
