#include <reproc/fill.h>

#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "macro.h"

int reproc_fill(reproc_t *process, reproc_filler in)
{
  return reproc_fill_bufsize(process, in, 4096);
}

int reproc_fill_bufsize(reproc_t *process,
                        reproc_filler in,
                        const size_t bufSize)
{
  ASSERT_EINVAL(process);
  ASSERT_EINVAL(in.function);

  bool more = true;
  int r = -1;

  uint8_t *buffer = malloc(bufSize);

  while (more) {

    size_t writeSize = 0;
    r = in.function(buffer, bufSize, &writeSize, &more, in.context);
    if (r != 0) {
      goto finish;
    }

    size_t total_written = 0;
    while (total_written < writeSize) {
      reproc_event_source source = { process, REPROC_EVENT_IN, 0 };

      r = reproc_poll(&source, 1, REPROC_INFINITE);
      if (r < 0) {
        goto finish;
      }

      if (source.events & REPROC_EVENT_DEADLINE) {
        r = REPROC_ETIMEDOUT;
        goto finish;
      }

      r = reproc_write(process, buffer, writeSize);
      if (r < 0) {
        goto finish;
      } else if (r == 0) {
        r = REPROC_EPIPE;
        goto finish;
      }

      total_written += (size_t) r;
    }
  }

finish:
  free(buffer);
  return r;
}

#define min(a, b) ((a) < (b) ? (a) : (b))

static int filler_buffer(uint8_t *const buffer,
                         const size_t bufSize,
                         size_t *written,
                         bool *more,
                         const void *context)
{
  ASSERT_EINVAL(context);

  reproc_filler_buffer_ctx *data = (reproc_filler_buffer_ctx *) context;

  if (data->offset >= data->size) {
    *written = 0;
    *more = false;
    return 0;
  }

  *written = min(data->size - data->offset, bufSize);
  memcpy(buffer, data->buffer + data->offset, *written);
  data->offset += *written;
  return 0;
}

reproc_filler reproc_filler_buffer(const reproc_filler_buffer_ctx *ctx)
{
  return (reproc_filler){ filler_buffer, ctx };
}
