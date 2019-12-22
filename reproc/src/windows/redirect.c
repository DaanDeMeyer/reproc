#include <redirect.h>

#include <pipe.h>

#include <assert.h>
#include <windows.h>

enum { FILE_NO_SHARE = 0, FILE_NO_TEMPLATE = 0 };

static const char *DEVNULL = "NUL";

static SECURITY_ATTRIBUTES INHERIT_HANDLE = { .nLength = sizeof(
                                                  SECURITY_ATTRIBUTES),
                                              .bInheritHandle = true,
                                              .lpSecurityDescriptor = NULL };

static const struct pipe_options CHILD_OPTIONS = { .inherit = true,
                                                   .nonblocking = false };
static const struct pipe_options PARENT_OPTIONS = { .inherit = false,
                                                    .nonblocking = true };

REPROC_ERROR
redirect_pipe(HANDLE *parent, HANDLE *child, REPROC_STREAM stream)
{
  assert(parent);
  assert(child);

  return stream == REPROC_STREAM_IN
             ? pipe_init(child, CHILD_OPTIONS, parent, PARENT_OPTIONS)
             : pipe_init(parent, PARENT_OPTIONS, child, CHILD_OPTIONS);
}

REPROC_ERROR
redirect_inherit(HANDLE *parent, HANDLE *child, REPROC_STREAM stream)
{
  assert(parent);
  assert(child);

  DWORD stream_id = stream == REPROC_STREAM_IN
                        ? STD_INPUT_HANDLE
                        : stream == REPROC_STREAM_OUT ? STD_OUTPUT_HANDLE
                                                      : STD_ERROR_HANDLE;
  HANDLE handle = HANDLE_INVALID;
  BOOL r = 0;

  HANDLE *stream_handle = GetStdHandle(stream_id);
  if (stream_handle == INVALID_HANDLE_VALUE) {
    return REPROC_ERROR_SYSTEM;
  }

  if (stream_handle == NULL) {
    return REPROC_ERROR_STREAM_CLOSED;
  }

  r = DuplicateHandle(GetCurrentProcess(), stream_handle, GetCurrentProcess(),
                      &handle, 0, true, DUPLICATE_SAME_ACCESS);
  if (r == 0) {
    return REPROC_ERROR_SYSTEM;
  }

  *parent = HANDLE_INVALID;
  *child = handle;

  return REPROC_SUCCESS;
}

REPROC_ERROR
redirect_discard(HANDLE *parent, HANDLE *child, REPROC_STREAM stream)
{
  assert(parent);
  assert(child);

  DWORD mode = stream == REPROC_STREAM_IN ? GENERIC_READ : GENERIC_WRITE;

  HANDLE handle = CreateFile(DEVNULL, mode, FILE_NO_SHARE, &INHERIT_HANDLE,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                             (HANDLE) FILE_NO_TEMPLATE);
  if (handle == INVALID_HANDLE_VALUE) {
    return REPROC_ERROR_SYSTEM;
  }

  *parent = HANDLE_INVALID;
  *child = handle;

  return REPROC_SUCCESS;
}
