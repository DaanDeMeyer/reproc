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

Because the context this function expects does not store the output size,
`strlen` is called each time data is read to calculate the current size of the
output. This might cause performance problems when draining processes that
produce a lot of output.

Similarly, this sink will not work on processes that have null terminators in
their output because `strlen` is used to calculate the current output size.

The `drain` example shows how to use `reproc_string_sink`.
```
*/
REPROC_EXPORT bool
reproc_sink_string(const uint8_t *buffer, unsigned int size, void *context);

/*!
Discards the output of a process.
*/
REPROC_EXPORT bool
reproc_sink_discard(const uint8_t *buffer, unsigned int size, void *context);

#ifdef __cplusplus
}
#endif
