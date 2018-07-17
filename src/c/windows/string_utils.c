#include "string_utils.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

REPROC_ERROR string_join(const char **string_array, int array_length,
                         char **result)
{
  assert(string_array);
  assert(array_length >= 0);
  assert(result);

  for (int i = 0; i < array_length; i++) {
    assert(string_array[i]);
  }

  // Determine length of resulting string first
  size_t string_length = 1; // Count NULL terminator
  for (int i = 0; i < array_length; i++) {
    string_length += strlen(string_array[i]);
    if (i < array_length - 1) { string_length++; } // Count whitespace
  }

  char *string = malloc(sizeof(char) * string_length);
  if (string == NULL) { return REPROC_MEMORY_ERROR; }

  char *current = string; // Keeps track of where we are in the result string
  for (int i = 0; i < array_length; i++) {
    size_t part_length = strlen(string_array[i]);
    strcpy(current, string_array[i]);
    current += part_length;

    // We add a space after every part string except for the last one
    if (i < array_length - 1) {
      *current = ' ';
      current += 1;
    }
  }

  *current = '\0';

  *result = string;

  return REPROC_SUCCESS;
}

REPROC_ERROR string_to_wstring(const char *string, wchar_t **result)
{
  assert(string);
  assert(result);

  // Determine wstring length (MultiByteToWideChar returns required size if last
  // two arguments are NULL and 0)
  SetLastError(0);
  int wstring_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           string, -1, NULL, 0);
  if (wstring_length == 0) {
    switch (GetLastError()) {
    case ERROR_NO_UNICODE_TRANSLATION: return REPROC_INVALID_UNICODE;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  wchar_t *wstring = malloc(sizeof(wchar_t) * wstring_length);
  if (!wstring) { return REPROC_MEMORY_ERROR; }

  // Now that we pass our allocated string and its length MultiByteToWideChar
  // will actually perform the conversion.
  SetLastError(0);
  int written = MultiByteToWideChar(CP_UTF8, 0, string, -1, wstring,
                                    wstring_length);
  if (written == 0) {
    free(wstring);
    switch (GetLastError()) {
    case ERROR_NO_UNICODE_TRANSLATION: return REPROC_INVALID_UNICODE;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  *result = wstring;

  return REPROC_SUCCESS;
}

REPROC_ERROR wstring_to_string(const wchar_t *wstring, char **result)
{
  // Same logic as string_to_wstring but in the opposite direction
  assert(wstring);
  assert(result);

  SetLastError(0);
  int string_length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                          wstring, -1, NULL, 0, NULL, NULL);
  if (string_length == 0) {
    switch (GetLastError()) {
    case ERROR_NO_UNICODE_TRANSLATION: return REPROC_INVALID_UNICODE;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  char *string = malloc(sizeof(char) * string_length);
  if (!string) { return REPROC_MEMORY_ERROR; }

  SetLastError(0);
  int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstring, -1,
                                    string, string_length, NULL, NULL);
  if (written == 0) {
    free(string);
    switch (GetLastError()) {
    case ERROR_NO_UNICODE_TRANSLATION: return REPROC_INVALID_UNICODE;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  *result = string;

  return REPROC_SUCCESS;
}
