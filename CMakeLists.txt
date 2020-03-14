cmake_minimum_required(VERSION 3.12...3.16)

# TODO: Remove once CMake 3.17 is released.
if(CMAKE_GENERATOR MATCHES "Unix Makefiles|Ninja")
  set(
    CMAKE_EXPORT_COMPILE_COMMANDS
    $ENV{CMAKE_EXPORT_COMPILE_COMMANDS}
    CACHE BOOL "Enable/Disable output of compile commands during generation."
  )
endif()

project(
  reproc
  VERSION 12.0.0
  DESCRIPTION "Cross-platform C99/C++11 process library"
  HOMEPAGE_URL "https://github.com/DaanDeMeyer/reproc"
  LANGUAGES C
)

# Common options and functions separated for easier reuse in other projects.
include(cmake/reproc.cmake)

option(REPROC++ "Build reproc++." ${REPROC_DEVELOP})
option(REPROC_TEST "Build tests." ${REPROC_DEVELOP})
option(REPROC_EXAMPLES "Build examples." ${REPROC_DEVELOP})

option(
  REPROC_MULTITHREADED
  "Use `pthread_sigmask` and link against the system's thread library"
  ON
)

if(REPROC++)
  enable_language(CXX)
endif()

if(REPROC_TEST)
  enable_testing()
endif()

if(REPROC_MULTITHREADED)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  set(REPROC_THREAD_LIBRARY ${CMAKE_THREAD_LIBS_INIT})
endif()

add_subdirectory(reproc)
if(REPROC++)
  add_subdirectory(reproc++)
endif()
