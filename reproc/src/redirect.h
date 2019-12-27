#pragma once

#include "handle.h"

typedef enum {
  REDIRECT_STREAM_IN,
  REDIRECT_STREAM_OUT,
  REDIRECT_STREAM_ERR
} REDIRECT_STREAM;

int redirect_pipe(handle *parent, handle *child, REDIRECT_STREAM stream);

int redirect_inherit(handle *parent, handle *child, REDIRECT_STREAM stream);

int redirect_discard(handle *parent, handle *child, REDIRECT_STREAM stream);
