#pragma once

#include <reproc/reproc.h>

#include <handle.h>

// Sets up the requested redirection type for the given stream and stores the
// resulting handles in `parent` and `child`.
//
// `parent` will only contain a valid handle if `type` is
// `REPROC_REDIRECT_PIPE`. `child` has to be duplicated onto its corresponding
// stream in the child process.
REPROC_ERROR redirect_pipe(handle *parent,
                           handle *child,
                           REPROC_STREAM stream);

REPROC_ERROR redirect_inherit(handle *parent,
                              handle *child,
                              REPROC_STREAM stream);

REPROC_ERROR redirect_discard(handle *parent,
                              handle *child,
                              REPROC_STREAM stream);
