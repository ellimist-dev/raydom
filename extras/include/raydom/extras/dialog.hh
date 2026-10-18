#pragma once

#include <raydom/extras/dialog.h>

#include <cstddef>
#include <string>
#include <vector>

namespace rdx {

struct DialogOptions {
  std::string                  title;
  std::vector<rdx_file_filter> filters;  // {"Images", "png,jpg"}
  std::string                  default_path;
  std::string                  default_name;
};

// status is OK, CANCEL or ERROR; on ERROR, error says why.
struct DialogResult {
  rdx_dialog_result        status = RDX_DIALOG_CANCEL;
  std::vector<std::string> paths;
  std::string              error;

  explicit operator bool() const { return status == RDX_DIALOG_OK; }
  const std::string& path() const {
    static const std::string none;
    return paths.empty() ? none : paths.front();
  }
};

namespace detail {
inline rdx_dialog_desc desc(const DialogOptions& o) {
  auto opt = [](const std::string& s) { return s.empty() ? nullptr : s.c_str(); };
  return {opt(o.title), o.filters.data(), o.filters.size(), opt(o.default_path), opt(o.default_name)};
}
inline DialogResult one(rdx_dialog_result r, char* path) {
  DialogResult out;
  out.status = r;
  if (path) out.paths.emplace_back(path);
  if (r == RDX_DIALOG_ERROR && rdx_dialog_error()) out.error = rdx_dialog_error();
  rdx_dialog_free(path);
  return out;
}
}  // namespace detail

// Blocking; see dialog.h.
inline DialogResult open_file(const DialogOptions& o = {}) {
  rdx_dialog_desc d    = detail::desc(o);
  char*           path = nullptr;
  return detail::one(rdx_dialog_open(&d, &path), path);
}
inline DialogResult save_file(const DialogOptions& o = {}) {
  rdx_dialog_desc d    = detail::desc(o);
  char*           path = nullptr;
  return detail::one(rdx_dialog_save(&d, &path), path);
}
inline DialogResult pick_folder(const DialogOptions& o = {}) {
  rdx_dialog_desc d    = detail::desc(o);
  char*           path = nullptr;
  return detail::one(rdx_dialog_pick_folder(&d, &path), path);
}
inline DialogResult open_files(const DialogOptions& o = {}) {
  rdx_dialog_desc d     = detail::desc(o);
  char**          paths = nullptr;
  std::size_t     count = 0;
  DialogResult    out;
  out.status = rdx_dialog_open_multiple(&d, &paths, &count);
  for (std::size_t i = 0; i < count; i++) out.paths.emplace_back(paths[i]);
  if (out.status == RDX_DIALOG_ERROR && rdx_dialog_error()) out.error = rdx_dialog_error();
  rdx_dialog_free_paths(paths, count);
  return out;
}

}  // namespace rdx
