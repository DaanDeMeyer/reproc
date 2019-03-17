#ifndef REPROC_POSIX_FD_H
#define REPROC_POSIX_FD_H

// If `fd` is not 0, closes `fd` and set `fd` to 0. Otherwise, does
// nothing. Does not overwrite `errno` if an error occurs while closing `fd`.
void fd_close(int *fd);

#endif
