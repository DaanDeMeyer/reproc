#pragma once

#ifndef REPROCXX_EXPORT
  #ifdef REPROCXX_STATIC
    #define REPROCXX_EXPORT
  #else
    #ifdef _WIN32
      #ifdef REPROCXX_BUILDING
        #define REPROCXX_EXPORT __declspec(dllexport)
      #else
        #define REPROCXX_EXPORT __declspec(dllimport)
      #endif
    #else
      #ifdef REPROCXX_BUILDING
        #define REPROCXX_EXPORT __attribute__((visibility("default")))
      #else
        #define REPROCXX_EXPORT
      #endif
    #endif
  #endif
#endif
