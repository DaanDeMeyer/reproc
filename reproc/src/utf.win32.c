#include "utf.h"

#include "error.h"

#include <limits.h>
#include <stdlib.h>
#include <windows.h>

wchar_t *utf16_from_utf8(const char *string, size_t size)
{
  ASSERT(string);
  // Overflow check although we really don't expect this to ever happen. This
  // makes the following casts to `int` safe.
  ASSERT(size <= INT_MAX);

  // Determine wstring size (`MultiByteToWideChar` returns the required size if
  // its last two arguments are `NULL` and 0).
  int r = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, string, (int) size,
                              NULL, 0);
  if (r == 0) {
    return NULL;
  }

  // `MultiByteToWideChar` does not return negative values so the cast to
  // `size_t` is safe.
  wchar_t *wstring = calloc((size_t) r, sizeof(wchar_t));
  if (wstring == NULL) {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return NULL;
  }

  // Now we pass our allocated string and its size as the last two arguments
  // instead of `NULL` and 0 which makes `MultiByteToWideChar` actually perform
  // the conversion.
  r = MultiByteToWideChar(CP_UTF8, 0, string, (int) size, wstring, r);
  if (r == 0) {
    free(wstring);
    return NULL;
  }

  return wstring;
}
