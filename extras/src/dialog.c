#include <raydom/extras/dialog.h>
#include <stdlib.h>
#include <string.h>

static const char* last_error;

void rdx_dialog_free(char* path) { free(path); }

void rdx_dialog_free_paths(char** paths, size_t count) {
  if (!paths) return;
  for (size_t i = 0; i < count; i++) free(paths[i]);
  free((void*)paths);
}

const char* rdx_dialog_error(void) { return last_error; }

#ifndef RDX_DIALOG_NATIVE

static rdx_dialog_result unsupported(void) {
  last_error = "file dialogs are not supported in this build";
  return RDX_DIALOG_ERROR;
}

rdx_dialog_result rdx_dialog_open(const rdx_dialog_desc* desc, char** path) {
  (void)desc;
  if (path) *path = NULL;
  return unsupported();
}

rdx_dialog_result rdx_dialog_open_multiple(const rdx_dialog_desc* desc, char*** paths, size_t* count) {
  (void)desc;
  if (paths) *paths = NULL;
  if (count) *count = 0;
  return unsupported();
}

rdx_dialog_result rdx_dialog_save(const rdx_dialog_desc* desc, char** path) { return rdx_dialog_open(desc, path); }

rdx_dialog_result rdx_dialog_pick_folder(const rdx_dialog_desc* desc, char** path) {
  return rdx_dialog_open(desc, path);
}

#else

#include <raylib.h>

#include "external/nfd/nfd.h"

enum { MAX_FILTERS = 32 };

static const rdx_dialog_desc no_desc;

// Ties the dialog to the raylib window where the platform supports it. Linux is left unparented:
// raylib's handle there can be X11 or Wayland, and nothing says which.
static nfdwindowhandle_t parent(void) {
  nfdwindowhandle_t h = {NFD_WINDOW_HANDLE_TYPE_UNSET, NULL};
#if defined(__APPLE__) || defined(_WIN32)
  if (IsWindowReady()) {
#if defined(__APPLE__)
    h.type = NFD_WINDOW_HANDLE_TYPE_COCOA;
#else
    h.type = NFD_WINDOW_HANDLE_TYPE_WINDOWS;
#endif
    h.handle = GetWindowHandle();
  }
#endif
  return h;
}

static nfdfiltersize_t filters(const rdx_dialog_desc* d, nfdu8filteritem_t out[MAX_FILTERS]) {
  size_t n = d->filters ? d->filter_count : 0;
  if (n > MAX_FILTERS) n = MAX_FILTERS;
  for (size_t i = 0; i < n; i++) out[i] = (nfdu8filteritem_t){d->filters[i].name, d->filters[i].extensions};
  return (nfdfiltersize_t)n;
}

static char* copy(const char* s) {
  size_t n = strlen(s) + 1;
  char*  p = malloc(n);
  if (p) memcpy(p, s, n);
  return p;
}

// NFD allocates with its own allocator on some platforms; results are copied so rdx_dialog_free is free().
static rdx_dialog_result finish(nfdresult_t r, nfdu8char_t* out, char** path) {
  if (r == NFD_OKAY) {
    *path = copy(out);
    NFD_FreePathU8(out);
    if (!*path) {
      last_error = "out of memory";
      return RDX_DIALOG_ERROR;
    }
    return RDX_DIALOG_OK;
  }
  if (r == NFD_CANCEL) return RDX_DIALOG_CANCEL;
  last_error = NFD_GetError();
  return RDX_DIALOG_ERROR;
}

static bool begin(char** path) {
  last_error = NULL;
  if (path) *path = NULL;
  if (!path) {
    last_error = "null output pointer";
    return false;
  }
  if (NFD_Init() != NFD_OKAY) {
    last_error = NFD_GetError();
    return false;
  }
  return true;
}

rdx_dialog_result rdx_dialog_open(const rdx_dialog_desc* desc, char** path) {
  if (!begin(path)) return RDX_DIALOG_ERROR;
  const rdx_dialog_desc* d = desc ? desc : &no_desc;
  nfdu8filteritem_t      f[MAX_FILTERS];
  nfdopendialogu8args_t  args = {.filterList   = f,
                                 .filterCount  = filters(d, f),
                                 .defaultPath  = d->default_path,
                                 .parentWindow = parent(),
                                 .title        = d->title};
  nfdu8char_t*           out  = NULL;
  rdx_dialog_result      r    = finish(NFD_OpenDialogU8_With(&out, &args), out, path);
  NFD_Quit();
  return r;
}

rdx_dialog_result rdx_dialog_save(const rdx_dialog_desc* desc, char** path) {
  if (!begin(path)) return RDX_DIALOG_ERROR;
  const rdx_dialog_desc* d = desc ? desc : &no_desc;
  nfdu8filteritem_t      f[MAX_FILTERS];
  nfdsavedialogu8args_t  args = {.filterList   = f,
                                 .filterCount  = filters(d, f),
                                 .defaultPath  = d->default_path,
                                 .defaultName  = d->default_name,
                                 .parentWindow = parent(),
                                 .title        = d->title};
  nfdu8char_t*           out  = NULL;
  rdx_dialog_result      r    = finish(NFD_SaveDialogU8_With(&out, &args), out, path);
  NFD_Quit();
  return r;
}

rdx_dialog_result rdx_dialog_pick_folder(const rdx_dialog_desc* desc, char** path) {
  if (!begin(path)) return RDX_DIALOG_ERROR;
  const rdx_dialog_desc* d    = desc ? desc : &no_desc;
  nfdpickfolderu8args_t  args = {.defaultPath = d->default_path, .parentWindow = parent(), .title = d->title};
  nfdu8char_t*           out  = NULL;
  rdx_dialog_result      r    = finish(NFD_PickFolderU8_With(&out, &args), out, path);
  NFD_Quit();
  return r;
}

rdx_dialog_result rdx_dialog_open_multiple(const rdx_dialog_desc* desc, char*** paths, size_t* count) {
  char* unused;
  if (count) *count = 0;
  if (paths) *paths = NULL;
  if (!paths || !count) {
    last_error = "null output pointer";
    return RDX_DIALOG_ERROR;
  }
  if (!begin(&unused)) return RDX_DIALOG_ERROR;
  const rdx_dialog_desc* d = desc ? desc : &no_desc;
  nfdu8filteritem_t      f[MAX_FILTERS];
  nfdopendialogu8args_t  args = {.filterList   = f,
                                 .filterCount  = filters(d, f),
                                 .defaultPath  = d->default_path,
                                 .parentWindow = parent(),
                                 .title        = d->title};
  const nfdpathset_t*    set  = NULL;
  nfdresult_t            nr   = NFD_OpenDialogMultipleU8_With(&set, &args);
  rdx_dialog_result      r    = nr == NFD_OKAY ? RDX_DIALOG_OK : nr == NFD_CANCEL ? RDX_DIALOG_CANCEL : RDX_DIALOG_ERROR;
  if (r == RDX_DIALOG_ERROR) last_error = NFD_GetError();
  if (r == RDX_DIALOG_OK) {
    nfdpathsetsize_t n = 0;
    char**           out = NULL;
    if (NFD_PathSet_GetCount(set, &n) == NFD_OKAY) out = (char**)calloc(n ? n : 1, sizeof *out);
    if (!out) {
      last_error = "couldn't read the selection";
      r          = RDX_DIALOG_ERROR;
    }
    for (nfdpathsetsize_t i = 0; r == RDX_DIALOG_OK && i < n; i++) {
      nfdu8char_t* p = NULL;
      if (NFD_PathSet_GetPathU8(set, i, &p) == NFD_OKAY) out[i] = copy(p);
      if (!out[i]) {
        last_error = "couldn't read the selection";
        r          = RDX_DIALOG_ERROR;
      }
      if (p) NFD_PathSet_FreePathU8(p);
    }
    if (r == RDX_DIALOG_OK) {
      *paths = out;
      *count = (size_t)n;
    } else {
      rdx_dialog_free_paths(out, (size_t)n);
    }
    NFD_PathSet_Free(set);
  }
  NFD_Quit();
  return r;
}

#endif
