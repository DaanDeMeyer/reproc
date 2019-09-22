#include <reproc/sink.h>

#include <stdlib.h>
#include <string.h>

bool reproc_sink_string(const uint8_t *buffer, unsigned int size, void *context)
{
  char **string = (char **) context;
  size_t string_size = *string == NULL ? 0 : strlen(*string);

  char *realloc_result = realloc(*string, string_size + size + 1);
  if (!realloc_result) {
    free(*string);
    *string = NULL;
    return false;
  } else {
    *string = realloc_result;
  }

  memcpy(*string + string_size, buffer, size);

  (*string)[string_size + size] = '\0';

  return true;
}
