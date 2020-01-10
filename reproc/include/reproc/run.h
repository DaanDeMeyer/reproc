#pragma once

#include <reproc/reproc.h>
#include <reproc/sink.h>

/*!
Wrapper function that starts a process with the given arguments, drain its
output and waits until it exits. Have a look at its (trivial) implementation and
the documentation of the functions it calls to see exactly what it does:
https://github.com/DaanDeMeyer/reproc/blob/master/reproc/src/run.c
*/
REPROC_EXPORT int reproc_run(const char *const *argv,
                             reproc_options options,
                             reproc_sink out,
                             reproc_sink err);
