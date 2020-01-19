#pragma once

#include "handle.h"
#include "pipe.h"

typedef enum {
  REDIRECT_STREAM_IN = 1 << 0,
  REDIRECT_STREAM_OUT = 1 << 1,
  REDIRECT_STREAM_ERR = 1 << 2
} REDIRECT_STREAM;

int redirect_inherit(pipe_type *parent,
                     handle_type *child,
                     REDIRECT_STREAM stream);

int redirect_discard(pipe_type *parent,
                     handle_type *child,
                     REDIRECT_STREAM stream);
