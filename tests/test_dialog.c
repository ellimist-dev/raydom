// Only the paths that don't open a dialog: argument errors and freeing.
#include <raydom/extras/dialog.h>

#include "check.h"

int main(void) {
  CHECK(rdx_dialog_open(NULL, NULL) == RDX_DIALOG_ERROR && rdx_dialog_error());
  CHECK(rdx_dialog_save(NULL, NULL) == RDX_DIALOG_ERROR);
  CHECK(rdx_dialog_pick_folder(NULL, NULL) == RDX_DIALOG_ERROR);
  size_t count = 7;
  CHECK(rdx_dialog_open_multiple(NULL, NULL, &count) == RDX_DIALOG_ERROR && count == 0);
  rdx_dialog_free(NULL);
  rdx_dialog_free_paths(NULL, 0);
  return TEST_RESULT();
}
