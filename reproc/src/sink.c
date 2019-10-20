#include <reproc/sink.h>

#include <stdlib.h>
#include <string.h>

bool reproc_sink_string(REPROC_STREAM stream,
                        const uint8_t *buffer,
                        unsigned int size,
                        void *context)
{
  (void) stream;

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

bool reproc_sink_discard(REPROC_STREAM stream,
                         const uint8_t *buffer,
                         unsigned int size,
                         void *context)
{
  (void) stream;
  (void) buffer;
  (void) size;
  (void) context;

  return true;
}
