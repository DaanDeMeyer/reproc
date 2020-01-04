#include <reproc/reproc.h>

#include "error.h"
#include "macro.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <windows.h>

const int REPROC_EINVAL = -ERROR_INVALID_PARAMETER;
const int REPROC_EPIPE = -ERROR_BROKEN_PIPE;
const int REPROC_ETIMEDOUT = -WAIT_TIMEOUT;
const int REPROC_ENOMEM = ERROR_NOT_ENOUGH_MEMORY;

int error_unify(int r)
{
  return error_unify_or_else(r, 0);
}

int error_unify_or_else(int r, int success)
{
  assert(GetLastError() <= INT_MAX);
  return r < 0 ? r : r == 0 ? -(int) GetLastError() : success;
}

enum { ERROR_STRING_MAX_SIZE = 512 };

const char *error_string(int error)
{
  static wchar_t *wstring = NULL;
  int r = 0;

  wstring = malloc(sizeof(wchar_t) * ERROR_STRING_MAX_SIZE);
  if (wstring == NULL) {
    return "Failed to allocate memory for error string";
  }

  // We don't expect message sizes larger than the maximum possible int.
  r = (int) FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, (DWORD) abs(error),
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), wstring,
                           ERROR_STRING_MAX_SIZE, NULL);
  if (r == 0) {
    free(wstring);
    return "Failed to retrieve error string";
  }

  static THREAD_LOCAL char string[ERROR_STRING_MAX_SIZE];

  r = WideCharToMultiByte(CP_UTF8, 0, wstring, -1, string, ARRAY_SIZE(string),
                          NULL, NULL);

  free(wstring);

  if (r == 0) {
    return "Failed to convert error string to UTF-8";
  }

  // Remove trailing whitespace and period.
  if (r >= 4) {
    string[r - 4] = '\0';
  }

  return string;
}
