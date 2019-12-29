#pragma once

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#if defined(_WIN32) && !defined(__MINGW32__)
  #define THREAD_LOCAL __declspec(thread)
#else
  #define THREAD_LOCAL __thread
#endif
