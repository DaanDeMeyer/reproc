#include "redirect.h"

#include "error.h"
#include "pipe.h"

#include <assert.h>
#include <windows.h>

int redirect_pipe(pipe_type *parent, HANDLE *child, REDIRECT_STREAM stream)
{
  assert(parent);
  assert(child);

  return stream == REDIRECT_STREAM_IN ? pipe_init((pipe_type *) child, parent)
                                      : pipe_init(parent, (pipe_type *) child);
}

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

int redirect_inherit(pipe_type *parent, HANDLE *child, REDIRECT_STREAM stream)
{
  assert(parent);
  assert(child);

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

  *parent = PIPE_INVALID;
  *child = duplicated;

  return 0;
}

enum { FILE_NO_SHARE = 0, FILE_NO_TEMPLATE = 0 };

static SECURITY_ATTRIBUTES INHERIT = { .nLength = sizeof(SECURITY_ATTRIBUTES),
                                       .bInheritHandle = true,
                                       .lpSecurityDescriptor = NULL };

int redirect_discard(pipe_type *parent, HANDLE *child, REDIRECT_STREAM stream)
{
  assert(parent);
  assert(child);

  DWORD mode = stream == REDIRECT_STREAM_IN ? GENERIC_READ : GENERIC_WRITE;
  int r = 0;

  HANDLE handle = CreateFile("NUL", mode, FILE_NO_SHARE, &INHERIT,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                             (HANDLE) FILE_NO_TEMPLATE);
  if (handle == INVALID_HANDLE_VALUE) {
    return error_unify(r);
  }

  *parent = PIPE_INVALID;
  *child = handle;

  return 0;
}
