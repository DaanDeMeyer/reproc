#include "redirect.h"

#include "error.h"
#include "pipe.h"

#include <assert.h>
#include <windows.h>

static DWORD stream_to_id(REDIRECT_STREAM stream)
{
  switch (stream) {
    case REDIRECT_STREAM_IN:
      return STD_INPUT_HANDLE;
    case REDIRECT_STREAM_OUT:
      return STD_OUTPUT_HANDLE;
    case REDIRECT_STREAM_ERR:
      return STD_ERROR_HANDLE;
  }

  return 0;
}

int redirect_inherit(HANDLE *handle, REDIRECT_STREAM stream)
{
  assert(handle);

  int r = 0;

  DWORD id = stream_to_id(stream);
  if (id == 0) {
    return -ERROR_INVALID_PARAMETER;
  }

  HANDLE *handle = GetStdHandle(id);
  if (handle == INVALID_HANDLE_VALUE) {
    return error_unify(r);
  }

  if (handle == NULL) {
    return -ERROR_BROKEN_PIPE;
  }

  HANDLE duplicated = HANDLE_INVALID;
  r = DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
                      &duplicated, 0, true, DUPLICATE_SAME_ACCESS);
  if (r == 0) {
    return error_unify(r);
  }

  *handle = duplicated;

  return 0;
}

enum { FILE_NO_SHARE = 0, FILE_NO_TEMPLATE = 0 };

static SECURITY_ATTRIBUTES INHERIT = { .nLength = sizeof(SECURITY_ATTRIBUTES),
                                       .bInheritHandle = true,
                                       .lpSecurityDescriptor = NULL };

int redirect_discard(HANDLE *handle, REDIRECT_STREAM stream)
{
  assert(handle);

  DWORD mode = stream == REDIRECT_STREAM_IN ? GENERIC_READ : GENERIC_WRITE;
  int r = 0;

  HANDLE handle = CreateFile("NUL", mode, FILE_NO_SHARE, &INHERIT,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                             (HANDLE) FILE_NO_TEMPLATE);
  if (handle == INVALID_HANDLE_VALUE) {
    return error_unify(r);
  }

  *handle = handle;

  return 0;
}
