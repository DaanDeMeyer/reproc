#pragma once

#include "handle.h"
#include "pipe.h"

// Keep in sync with `REPROC_STREAM`.
typedef enum {
  REDIRECT_STREAM_IN,
  REDIRECT_STREAM_OUT,
  REDIRECT_STREAM_ERR
} REDIRECT_STREAM;

int redirect_inherit(handle_type *handle, REDIRECT_STREAM stream);

int redirect_discard(handle_type *handle, REDIRECT_STREAM stream);
