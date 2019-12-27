#pragma once

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#if defined(__MINGW32__)
  #define THREAD_LOCAL __thread
#elif defined(_WIN32)
  #define THREAD_LOCAL __declspec(thread)
#endif
