#include <windows/redirect.h>

#include <windows/pipe.h>

#include <assert.h>
#include <stddef.h>
#include <windows.h>

#define FILE_NO_SHARE 0
#define FILE_NO_TEMPLATE NULL

static const char *DEVNULL = "NUL";

static SECURITY_ATTRIBUTES INHERIT_HANDLE = { .nLength = sizeof(
                                                  SECURITY_ATTRIBUTES),
                                              .bInheritHandle = false,
                                              .lpSecurityDescriptor = NULL };
REPROC_ERROR
redirect(HANDLE *parent,
         HANDLE *child,
         REPROC_STREAM stream,
         REPROC_REDIRECT type)
{
  assert(parent);
  assert(child);

  const struct pipe_options child_blocking = { .inherit = true,
                                               .overlapped = false };
  const struct pipe_options parent_overlapped = { .inherit = false,
                                                  .overlapped = true };

  *parent = NULL;

  switch (stream) {

    case REPROC_STREAM_IN:

      switch (type) {
        case REPROC_REDIRECT_PIPE:
          return pipe_init(child, child_blocking, parent, parent_overlapped);

        case REPROC_REDIRECT_INHERIT:
          *child = GetStdHandle(STD_INPUT_HANDLE);
          return *child == INVALID_HANDLE_VALUE ? REPROC_ERROR_SYSTEM
                                                : REPROC_SUCCESS;

        case REPROC_REDIRECT_DISCARD:
          *child = CreateFile(DEVNULL, GENERIC_READ, FILE_NO_SHARE,
                              &INHERIT_HANDLE, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, FILE_NO_TEMPLATE);
          return *child == INVALID_HANDLE_VALUE ? REPROC_ERROR_SYSTEM
                                                : REPROC_SUCCESS;
      }

      assert(false);
      break;

    case REPROC_STREAM_OUT:
    case REPROC_STREAM_ERR:

      switch (type) {
        case REPROC_REDIRECT_PIPE:
          return pipe_init(parent, parent_overlapped, child, child_blocking);

        case REPROC_REDIRECT_INHERIT:
          *child = GetStdHandle(stream == REPROC_STREAM_OUT ? STD_OUTPUT_HANDLE
                                                            : STD_ERROR_HANDLE);
          return *child == INVALID_HANDLE_VALUE ? REPROC_ERROR_SYSTEM
                                                : REPROC_SUCCESS;

        case REPROC_REDIRECT_DISCARD:
          *child = CreateFile(DEVNULL, GENERIC_WRITE, FILE_NO_SHARE,
                              &INHERIT_HANDLE, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, FILE_NO_TEMPLATE);
          return *child == INVALID_HANDLE_VALUE ? REPROC_ERROR_SYSTEM
                                                : REPROC_SUCCESS;
      }
  }

  assert(false);
  return REPROC_ERROR_SYSTEM;
}
