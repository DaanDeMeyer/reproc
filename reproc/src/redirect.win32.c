#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "redirect.h"

#include "error.h"
#include "pipe.h"

#include <assert.h>
#include <windows.h>

static DWORD stream_to_id(REPROC_STREAM stream)
{
  switch (stream) {
    case REPROC_STREAM_IN:
      return STD_INPUT_HANDLE;
    case REPROC_STREAM_OUT:
      return STD_OUTPUT_HANDLE;
    case REPROC_STREAM_ERR:
      return STD_ERROR_HANDLE;
  }

  return 0;
}

int redirect_parent(HANDLE *out, REPROC_STREAM stream)
{
  assert(out);

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

  *out = handle;

  return 0;
}

enum { FILE_NO_SHARE = 0, FILE_NO_TEMPLATE = 0 };

static SECURITY_ATTRIBUTES INHERIT = { .nLength = sizeof(SECURITY_ATTRIBUTES),
                                       .bInheritHandle = true,
                                       .lpSecurityDescriptor = NULL };

int redirect_discard(HANDLE *out, REPROC_STREAM stream)
{
  assert(out);

  DWORD mode = stream == REPROC_STREAM_IN ? GENERIC_READ : GENERIC_WRITE;
  int r = 0;

  HANDLE handle = CreateFile("NUL", mode, FILE_NO_SHARE, &INHERIT,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                             (HANDLE) FILE_NO_TEMPLATE);
  if (handle == INVALID_HANDLE_VALUE) {
    return error_unify(r);
  }

  *out = handle;

  return 0;
}

int redirect_file(FILE *file, HANDLE *handle)
{
  int r = -1;

  r = _fileno(file);
  if (r < 0) {
    return -ERROR_INVALID_HANDLE;
  }

  intptr_t result = _get_osfhandle(r);
  if (result == -1) {
    return -ERROR_INVALID_HANDLE;
  }

  *handle = (HANDLE) result;

  return error_unify(r);
}
