#include <reproc/reproc.h>

#include "error.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

const int REPROC_EINVAL = -EINVAL;
const int REPROC_EPIPE = -EPIPE;
const int REPROC_ETIMEDOUT = -ETIMEDOUT;
const int REPROC_EINPROGRESS = -EINPROGRESS;
const int REPROC_ENOMEM = -ENOMEM;

int error_unify(int r)
{
  return error_unify_or_else(r, 0);
}

int error_unify_or_else(int r, int success)
{
  return r < -1 ? r : r == -1 ? -errno : success;
}

const char *error_string(int error)
{
  return strerror(abs(error));
}
