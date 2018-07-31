#ifndef REPROC_EXPORT_H
#define REPROC_EXPORT_H

#if defined(_WIN32) && defined(REPROC_SHARED)
#if defined(REPROC_BUILDING)
#define REPROC_EXPORT __declspec(dllexport)
#else
#define REPROC_EXPORT __declspec(dllimport)
#endif
#else
#define REPROC_EXPORT __attribute__ ((visibility("default")))
#endif

#endif
