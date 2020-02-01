#pragma once

#include <stdio.h>
#include <stdlib.h>

#define ASSERT(expression)                                                     \
  do {                                                                         \
    if (!(expression)) {                                                       \
      fprintf(stderr, "%s:%u: Assertion '%s' failed ", __FILE__, __LINE__,     \
              #expression);                                                    \
                                                                               \
      fprintf(stderr, "(r == %i", abs(r));                                     \
                                                                               \
      if (r < 0) {                                                             \
        fprintf(stderr, ": %s", reproc_strerror(r));                           \
      }                                                                        \
                                                                               \
      fprintf(stderr, ")");                                                    \
                                                                               \
      fflush(stderr);                                                          \
                                                                               \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)
