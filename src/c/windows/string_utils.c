#include "string_utils.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

PROCESS_LIB_ERROR string_join(const char **string_array, int array_length,
                              char **result)
{
  assert(string_array);
  assert(array_length >= 0);
  assert(result);

  for (int i = 0; i < array_length; i++) {
    assert(string_array[i]);
  }

  size_t string_length = 1; // Null terminator
  for (int i = 0; i < array_length; i++) {
    string_length += strlen(string_array[i]);
    if (i < array_length - 1) { string_length++; } // whitespace
  }

  char *string = malloc(sizeof(char) * string_length);
  if (string == NULL) { return PROCESS_LIB_MEMORY_ERROR; }

  char *current = string; // Keeps track of where we are in the result string
  for (int i = 0; i < array_length; i++) {
    size_t part_length = strlen(string_array[i]);
    strcpy(current, string_array[i]);
    current += part_length;

    if (i < array_length - 1) {
      *current = ' ';
      current += 1;
    }
  }

  *current = '\0';

  *result = string;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR string_to_wstring(const char *string, wchar_t **result)
{
  assert(string);
  assert(result);

  SetLastError(0);
  int wstring_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           string, -1, NULL, 0);
  if (wstring_length == 0) {
    switch (GetLastError()) {
    case ERROR_NO_UNICODE_TRANSLATION: return PROCESS_LIB_INVALID_UNICODE;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  wchar_t *wstring = malloc(sizeof(wchar_t) * wstring_length);
  if (!wstring) { return PROCESS_LIB_MEMORY_ERROR; }

  SetLastError(0);
  int written = MultiByteToWideChar(CP_UTF8, 0, string, -1, wstring,
                                    wstring_length);
  if (written == 0) {
    free(wstring);
    switch (GetLastError()) {
    case ERROR_NO_UNICODE_TRANSLATION: return PROCESS_LIB_INVALID_UNICODE;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  *result = wstring;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR wstring_to_string(const wchar_t *wstring, char **result)
{
  assert(wstring);
  assert(result);

  SetLastError(0);
  int string_length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                          wstring, -1, NULL, 0, NULL, NULL);
  if (string_length == 0) {
    switch (GetLastError()) {
    case ERROR_NO_UNICODE_TRANSLATION: return PROCESS_LIB_INVALID_UNICODE;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  char *string = malloc(sizeof(char) * string_length);
  if (!string) { return PROCESS_LIB_MEMORY_ERROR; }

  SetLastError(0);
  int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstring, -1,
                                    string, string_length, NULL, NULL);
  if (written == 0) {
    free(string);
    switch(GetLastError()) {
    case ERROR_NO_UNICODE_TRANSLATION: return PROCESS_LIB_INVALID_UNICODE;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  *result = string;

  return PROCESS_LIB_SUCCESS;
}
