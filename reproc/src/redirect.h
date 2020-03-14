#pragma once

#include <reproc/reproc.h>

#include "handle.h"
#include "pipe.h"

int redirect_init(pipe_type *parent,
                  handle_type *child,
                  REPROC_STREAM stream,
                  reproc_redirect redirect,
                  bool nonblocking,
                  handle_type out);

handle_type redirect_destroy(handle_type handle, REPROC_REDIRECT type);

// Internal prototypes

int redirect_parent(handle_type *out, REPROC_STREAM stream);

int redirect_discard(handle_type *out, REPROC_STREAM stream);

int redirect_file(FILE *file, handle_type *handle);
