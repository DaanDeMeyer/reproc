#include <reproc/error.h>

#include <windows.h>

unsigned int reproc_error_system(void)
{
  return GetLastError();
}

enum { ERROR_STRING_MAX_SIZE = 512 };

const char *reproc_error_string(REPROC_ERROR error)
{
  switch (error) {
    case REPROC_SUCCESS:
      return "success";
    case REPROC_ERROR_WAIT_TIMEOUT:
      return "wait timeout";
    case REPROC_ERROR_STREAM_CLOSED:
      return "stream closed";
    case REPROC_ERROR_SYSTEM: {
      static wchar_t wstring[ERROR_STRING_MAX_SIZE];

      // We don't expect message sizes larger than the maximum possible int.
      int rv = (int) FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                                        FORMAT_MESSAGE_IGNORE_INSERTS,
                                    NULL, reproc_error_system(),
                                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                    wstring, ERROR_STRING_MAX_SIZE, NULL);
      if (rv == 0) {
        return "Failed to retrieve error string";
      }

      static char string[ERROR_STRING_MAX_SIZE];

      rv = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstring, -1,
                               string, ERROR_STRING_MAX_SIZE, NULL, NULL);
      if (rv == 0) {
        return "Failed to convert error string to UTF-8";
      }

      // Remove trailing whitespace and period.
      if (rv >= 4) {
        string[rv - 4] = '\0';
      }

      return string;
    }
  }

  return "unknown error";
}
