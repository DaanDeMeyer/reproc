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
redirect(HANDLE *parent,
         HANDLE *child,
         REPROC_STREAM stream,
         REPROC_REDIRECT type)
{
  assert(parent);
  assert(child);

  *parent = NULL;

  switch (stream) {

    case REPROC_STREAM_IN:

      switch (type) {
        case REPROC_REDIRECT_PIPE:
          return pipe_init(child, CHILD_OPTIONS, parent, PARENT_OPTIONS);

        case REPROC_REDIRECT_INHERIT:
          *child = GetStdHandle(STD_INPUT_HANDLE);
          return *child == INVALID_HANDLE_VALUE ? REPROC_ERROR_SYSTEM
                                                : REPROC_SUCCESS;

        case REPROC_REDIRECT_DISCARD:
          *child = CreateFile(DEVNULL, GENERIC_READ, FILE_NO_SHARE,
                              &INHERIT_HANDLE, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, (HANDLE) FILE_NO_TEMPLATE);
          return *child == INVALID_HANDLE_VALUE ? REPROC_ERROR_SYSTEM
                                                : REPROC_SUCCESS;
      }

      assert(false);
      break;

    case REPROC_STREAM_OUT:
    case REPROC_STREAM_ERR:

      switch (type) {
        case REPROC_REDIRECT_PIPE:
          return pipe_init(parent, PARENT_OPTIONS, child, CHILD_OPTIONS);

        case REPROC_REDIRECT_INHERIT:
          *child = GetStdHandle(stream == REPROC_STREAM_OUT ? STD_OUTPUT_HANDLE
                                                            : STD_ERROR_HANDLE);
          return *child == INVALID_HANDLE_VALUE ? REPROC_ERROR_SYSTEM
                                                : REPROC_SUCCESS;

        case REPROC_REDIRECT_DISCARD:
          *child = CreateFile(DEVNULL, GENERIC_WRITE, FILE_NO_SHARE,
                              &INHERIT_HANDLE, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, (HANDLE) FILE_NO_TEMPLATE);
          return *child == INVALID_HANDLE_VALUE ? REPROC_ERROR_SYSTEM
                                                : REPROC_SUCCESS;
      }
  }

  assert(false);
  return REPROC_ERROR_SYSTEM;
}
