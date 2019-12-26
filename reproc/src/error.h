#pragma once

#if defined(_WIN32)
#define PROTECT_SYSTEM_ERROR(expression)                                       \
  do {                                                                         \
    DWORD saved_system_error = GetLastError();                                 \
    (void) !(expression);                                                      \
    SetLastError(saved_system_error);                                          \
  } while (0)
#else
#define PROTECT_SYSTEM_ERROR(expression)                                       \
  do {                                                                         \
    int saved_system_error = errno;                                            \
    (void) !(expression);                                                      \
    errno = saved_system_error;                                                \
  } while (0)
#endif

int error_unify(int r, int success);

const char *error_string(int error);
