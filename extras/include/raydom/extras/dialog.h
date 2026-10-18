#ifndef RAYDOM_EXTRAS_DIALOG_H
#define RAYDOM_EXTRAS_DIALOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The operating system's file dialogs: Cocoa panels on macOS, the shell dialog on Windows, and
// xdg-desktop-portal on Linux. Each call blocks until the user answers, and the window doesn't
// redraw meanwhile. Call from the main thread after InitWindow, for example from a click listener.
// Builds without dialog support (the web, or RAYDOM_FILE_DIALOG=OFF) return RDX_DIALOG_ERROR.

typedef enum rdx_dialog_result : uint8_t {
  RDX_DIALOG_OK,
  RDX_DIALOG_CANCEL,
  RDX_DIALOG_ERROR,  // rdx_dialog_error says why
} rdx_dialog_result;

typedef struct rdx_file_filter {
  const char* name;        // "Images"
  const char* extensions;  // "png,jpg": comma-separated, without dots
} rdx_file_filter;

// Every field is optional; a null desc uses the defaults.
typedef struct rdx_dialog_desc {
  const char*            title;
  const rdx_file_filter* filters;
  size_t                 filter_count;
  const char*            default_path;  // starting folder
  const char*            default_name;  // save: suggested file name
} rdx_dialog_desc;

// Paths are UTF-8 and owned by the caller: free them with rdx_dialog_free and rdx_dialog_free_paths.
rdx_dialog_result rdx_dialog_open(const rdx_dialog_desc* desc, char** path);
rdx_dialog_result rdx_dialog_open_multiple(const rdx_dialog_desc* desc, char*** paths, size_t* count);
rdx_dialog_result rdx_dialog_save(const rdx_dialog_desc* desc, char** path);
rdx_dialog_result rdx_dialog_pick_folder(const rdx_dialog_desc* desc, char** path);

void rdx_dialog_free(char* path);
void rdx_dialog_free_paths(char** paths, size_t count);

// Why the last call returned RDX_DIALOG_ERROR. Null when it didn't.
const char* rdx_dialog_error(void);

#ifdef __cplusplus
}
#endif

#endif
