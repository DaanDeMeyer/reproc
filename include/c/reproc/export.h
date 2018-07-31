#ifndef REPROC_EXPORT_H
#define REPROC_EXPORT_H

#if defined(REPROC_SHARED)
#  if defined(_WIN32)
#    if defined(REPROC_BUILDING)
#      define REPROC_EXPORT __declspec(dllexport)
#    else
#      define REPROC_EXPORT __declspec(dllimport)
#    endif
#  else
#    if defined(REPROC_BUILDING)
#      define REPROC_EXPORT __attribute__((visibility("default")))
#    else
#      define REPROC_EXPORT
#    endif
#  endif
#else
#  define REPROC_EXPORT
#endif

#endif
