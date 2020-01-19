#pragma once

#include "handle.h"
#include "pipe.h"

typedef enum {
  REDIRECT_STREAM_IN,
  REDIRECT_STREAM_OUT,
  REDIRECT_STREAM_ERR
} REDIRECT_STREAM;

int redirect_inherit(pipe_type *parent,
                     handle_type *child,
                     REDIRECT_STREAM stream);

int redirect_discard(pipe_type *parent,
                     handle_type *child,
                     REDIRECT_STREAM stream);
