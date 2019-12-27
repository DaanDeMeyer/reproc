#include <reproc/reproc.h>

#include "error.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <windows.h>

enum { ERROR_STRING_MAX_SIZE = 512 };

const int REPROC_ERROR_INVALID_ARGUMENT = -ERROR_INVALID_PARAMETER;
const int REPROC_ERROR_STREAM_CLOSED = -ERROR_BROKEN_PIPE;
const int REPROC_ERROR_WAIT_TIMEOUT = -WAIT_TIMEOUT;
const int REPROC_ERROR_IN_PROGRESS = -WSAEINPROGRESS;

int error_unify(int r, int success)
{
  assert(GetLastError() <= INT_MAX);
  return r == 0 ? -(int) GetLastError() : success;
}

const char *error_string(int error)
{
  static wchar_t wstring[ERROR_STRING_MAX_SIZE];
  int r = 0;

  // We don't expect message sizes larger than the maximum possible int.
  r = (int) FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, (DWORD) abs(error),
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), wstring,
                           ERROR_STRING_MAX_SIZE, NULL);
  if (r == 0) {
    return "Failed to retrieve error string";
  }

  static char string[ERROR_STRING_MAX_SIZE];

  r = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstring, -1, string,
                          ERROR_STRING_MAX_SIZE, NULL, NULL);
  if (r == 0) {
    return "Failed to convert error string to UTF-8";
  }

  // Remove trailing whitespace and period.
  if (r >= 4) {
    string[r - 4] = '\0';
  }

  return string;
}
