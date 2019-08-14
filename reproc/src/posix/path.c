#include "path.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Returns true if the null-terminated string indicated by `path` is a relative
// path. A path is relative if any character except the first is a forward slash
// ('/').
bool path_is_relative(const char *path)
{
  return strlen(path) > 0 && path[0] != '/' && *strchr(path + 1, '/') != '\0';
}

// Prepends the null-terminated string indicated by `path` with the current
// working directory. The caller is responsible for freeing the result of this
// function. If an error occurs, `NULL` is returned and `errno` is set to
// indicate the error.
char *path_prepend_cwd(const char *path)
{
  size_t path_size = strlen(path);
  size_t cwd_size = PATH_MAX;

  // We always allocate sufficient space for `path` but do not include this
  // space in `cwd_size` so we can be sure that when `getcwd` succeeds there is
  // sufficient space left in `cwd` to append `path`.

  // +2 reserves space to add a null terminator and potentially a missing '/'
  // after the current working directory.
  char *cwd = malloc(sizeof(char) * cwd_size + path_size + 2);
  if (!cwd) {
    return NULL;
  }

  while (getcwd(cwd, cwd_size) == NULL) {
    if (errno != ERANGE) {
      free(cwd);
      return NULL;
    }

    cwd_size += PATH_MAX;

    char *result = realloc(cwd, cwd_size + path_size + 1);
    if (result == NULL) {
      free(cwd);
      return NULL;
    }

    cwd = result;
  }

  cwd_size = strlen(cwd);

  // Add a forward slash after `cwd` if there is none.
  if (cwd[cwd_size - 1] != '/') {
    cwd[cwd_size] = '/';
    cwd[cwd_size + 1] = '\0';
    cwd_size++;
  }

  // Manual copy to avoid Clang Analyzer `strcpy` warning. We've made sure `cwd`
  // has sufficient space left to append `path` and a null terminator so this is
  // safe.
  for (size_t i = 0; i < path_size; i++) {
    cwd[cwd_size + i] = path[i];
  }

  cwd[cwd_size + path_size] = '\0';

  return cwd;
}
