#pragma once

#include <stdio.h>
#include <stdlib.h>

#define assert(expression)                                                     \
  if (!(expression)) {                                                         \
    fprintf(stderr, "%s:%u: Assertion '%s' failed.", __FILE__, __LINE__,       \
            #expression);                                                      \
    exit(1);                                                                   \
  }
