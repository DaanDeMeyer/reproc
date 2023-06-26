#pragma once

#include <reproc/reproc.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! Used by `reproc_fill` to provide data to the caller. Each time data is
read, `function` is called with `context`. If a filler returns a non-zero value,
`reproc_fill` will return immediately with the same value.

The function must set "written" equal to the number of bytes written to the
buffer and must set "more" to false when there is no more data to write.
*/
typedef struct reproc_filler {
  int (*function)(uint8_t *const buffer,
                  const size_t bufSize,
                  size_t *written,
                  bool *more,
                  const void *context);
  const void *context;
} reproc_filler;

/*!
Writes to the child process stdin until an error occurs or filler is exhausted.
The `in` filler provides the input for stdin.  The filler is called repeatedly.

Actionable errors:
- `REPROC_ETIMEDOUT`
*/
REPROC_EXPORT int reproc_fill(reproc_t *process, reproc_filler in);

/*! reproc_fill() but with a custom buffer size */
REPROC_EXPORT int
reproc_fill_bufsize(reproc_t *process, reproc_filler in, const size_t bufSize);

/*! A buffer pointer, size, and current offset */
typedef struct reproc_filler_buffer_ctx {
  const uint8_t *buffer;
  const size_t size;
  size_t offset;
} reproc_filler_buffer_ctx;

/*!
Writes the input of a process (stdin) to the value of a buffer given in `ctx`.
The `fill` example shows how to use `reproc_filler_buffer`.
*/
REPROC_EXPORT reproc_filler
reproc_filler_buffer(const reproc_filler_buffer_ctx *ctx);

#ifdef __cplusplus
}
#endif
