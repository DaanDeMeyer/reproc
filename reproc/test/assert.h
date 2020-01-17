#pragma once

#include <stdio.h>
#include <stdlib.h>

#define assert(expression)                                                     \
  do {                                                                         \
    if (!(expression)) {                                                       \
      fprintf(stderr, "%s:%u: Assertion '%s' failed ", __FILE__, __LINE__,     \
              #expression);                                                    \
                                                                               \
      if (r < 0) {                                                             \
        fprintf(stderr, "(r == %s)", reproc_strerror(r));                      \
      }                                                                        \
                                                                               \
      fflush(stderr);                                                          \
                                                                               \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)
