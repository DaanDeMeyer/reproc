/*! Sink functions that can be passed to `reproc_drain`. */

#pragma once

#include <reproc/reproc.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
Stores the output (both stdout and stderr) of a process in a single contiguous C
string.

Expects a `char **` with its value set to `NULL` as its initial context.
(Re)Allocates memory as necessary to store the output and assigns it as the
value of the given context. If allocating more memory fails, the already
allocated memory is freed and the value of the given context is set to `NULL`.

After calling `reproc_drain` with `reproc_sink_string`, the value of `context`
will either point to valid memory or will be set to `NULL`. This means it is
always safe to call `free` on `context`'s value after `reproc_drain` finishes.

Because the context this function expects does not store the output size,
`strlen` is called each time data is read to calculate the current size of the
output. This might cause performance problems when draining processes that
produce a lot of output.

Similarly, this sink will not work on processes that have null terminators in
their output because `strlen` is used to calculate the current output size.

The `drain` example shows how to use `reproc_sink_string`.
```
*/
static bool reproc_sink_string(REPROC_STREAM stream,
                               const uint8_t *buffer,
                               unsigned int size,
                               void *context);

/*! Discards the output of a process. */
static bool reproc_sink_discard(REPROC_STREAM stream,
                                const uint8_t *buffer,
                                unsigned int size,
                                void *context);

// We inline the sink implementations to avoid allocation/de-allocation issues
// over module (DLL) boundaries. By inlining the implementations, memory is
// always allocated and freed in the user's executable/DLL.

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

static inline bool reproc_sink_string(REPROC_STREAM stream,
                                      const uint8_t *buffer,
                                      unsigned int size,
                                      void *context)
{
  (void) stream;

  char **string = (char **) context;
  size_t string_size = *string == NULL ? 0 : strlen(*string);

  char *realloc_result = (char *) realloc(*string, string_size + size + 1);
  if (realloc_result == NULL) {
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

static inline bool reproc_sink_discard(REPROC_STREAM stream,
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

#pragma clang diagnostic pop

#ifdef __cplusplus
}
#endif
