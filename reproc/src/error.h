#pragma once

// Save the current error variable value and system error value and reapply them
// when `UNPROTECT_SYSTEM_ERROR` is called.

#if defined(_WIN32)
  #define PROTECT_SYSTEM_ERROR                                                 \
    BOOL saved_result = r;                                                     \
    DWORD saved_error = GetLastError()

  #define UNPROTECT_SYSTEM_ERROR                                               \
    do {                                                                       \
      r = saved_result;                                                        \
      SetLastError(saved_error);                                               \
    } while (0)
#else
  #define PROTECT_SYSTEM_ERROR                                                 \
    int saved_result = r;                                                      \
    int saved_error = errno

  #define UNPROTECT_SYSTEM_ERROR                                               \
    do {                                                                       \
      r = saved_result;                                                        \
      errno = saved_error;                                                     \
    } while (0)
#endif

// Use this to assert inside a `PROTECT/UNPROTECT` block to avoid clang dead
// store warnings.
#define assert_unused(expression)                                              \
  do {                                                                         \
    (void) !(expression);                                                      \
    assert((expression));                                                      \
  } while (0)

int error_unify(int r, int success);

const char *error_string(int error);
