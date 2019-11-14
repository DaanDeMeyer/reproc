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

static REPROC_ERROR
redirect_pipe(HANDLE *parent, HANDLE *child, REPROC_STREAM stream)
{
  assert(parent);
  assert(child);

  return stream == REPROC_STREAM_IN
             ? pipe_init(child, CHILD_OPTIONS, parent, PARENT_OPTIONS)
             : pipe_init(parent, PARENT_OPTIONS, child, CHILD_OPTIONS);
}

static REPROC_ERROR
redirect_inherit(HANDLE *parent, HANDLE *child, REPROC_STREAM stream)
{
  assert(parent);
  assert(child);

  *parent = NULL;
  HANDLE *stream_handle = NULL;

  DWORD stream_id = stream == REPROC_STREAM_IN
                        ? STD_INPUT_HANDLE
                        : stream == REPROC_STREAM_OUT ? STD_OUTPUT_HANDLE
                                                      : STD_ERROR_HANDLE;
  stream_handle = GetStdHandle(stream_id);
  if (stream_handle == INVALID_HANDLE_VALUE) {
    return REPROC_ERROR_SYSTEM;
  }

  if (stream_handle == NULL) {
    return REPROC_ERROR_STREAM_CLOSED;
  }

  BOOL rv = DuplicateHandle(GetCurrentProcess(), stream_handle,
                            GetCurrentProcess(), child, 0, true,
                            DUPLICATE_SAME_ACCESS);

  return rv == 0 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;
}

static REPROC_ERROR
redirect_discard(HANDLE *parent, HANDLE *child, REPROC_STREAM stream)
{
  assert(parent);
  assert(child);

  *parent = NULL;
  DWORD mode = stream == REPROC_STREAM_IN ? GENERIC_READ : GENERIC_WRITE;

  *child = CreateFile(DEVNULL, mode, FILE_NO_SHARE, &INHERIT_HANDLE,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                      (HANDLE) FILE_NO_TEMPLATE);

  return *child == INVALID_HANDLE_VALUE ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;
}

REPROC_ERROR
redirect(HANDLE *parent,
         HANDLE *child,
         REPROC_STREAM stream,
         REPROC_REDIRECT type)
{
  switch (type) {

    case REPROC_REDIRECT_PIPE:
      return redirect_pipe(parent, child, stream);

    case REPROC_REDIRECT_INHERIT:
      REPROC_ERROR error = redirect_inherit(parent, child, stream);
      // Discard if the corresponding parent stream is closed.
      return error == REPROC_ERROR_STREAM_CLOSED
                 ? redirect_discard(parent, child, stream)
                 : error;

    case REPROC_REDIRECT_DISCARD:
      return redirect_discard(parent, child, stream);
  }

  assert(false);
  return REPROC_ERROR_SYSTEM;
}
