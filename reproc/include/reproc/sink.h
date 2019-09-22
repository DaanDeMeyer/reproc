/*! Sink functions that can be passed to `reproc_drain`. */

#pragma once

#include <reproc/export.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
Stores the output of a process in a single contiguous C string.

Expects a `char **` with its value set to `NULL` as its initial context.
(Re)Allocates memory as necessary to store the output and assigns it as the
value of the given context. If allocating more memory fails, the already
allocated memory is freed and the value of the given context is set to `NULL`.

When `reproc_drain` finished after being called with `reproc_string_sink`, the
value of the given context will either point to valid memory or will be set to
`NULL`. This means it is always safe to call `free` on the context value after
`reproc_drain` finishes.

Example:

```c
char *output = NULL;
REPROC_ERROR error = reproc_drain(&process, REPROC_STREAM_OUT,
                                  reproc_sink_string, &output);
if (error) {
  // An error occurred in reproc.
  ...

  goto cleanup;
}

if (output == NULL) {
  // A memory allocation in `reproc_sink_string` failed.
  ...

  goto cleanup;
}

// `output` contains all the output of `process`.
...

cleanup:
free(output);
```
*/
REPROC_EXPORT bool
reproc_sink_string(const uint8_t *buffer, unsigned int size, void *context);

#ifdef __cplusplus
}
#endif
