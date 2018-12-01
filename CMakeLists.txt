# Every variable is prefixed with REPROC and everything except for the C library
# (reproc) is turned off by default to make reproc easy to use as a CMake
# subproject with add_subdirectory. All targets are also prefixed with "reproc-"
# to avoid name collisions.

# Needed for target_sources.
cmake_minimum_required(VERSION 3.1)

# Use updated policies on newer versions of CMake.
if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
  cmake_policy(VERSION 3.12)
endif()

project(reproc VERSION 2.0.0 LANGUAGES C)

include(${PROJECT_SOURCE_DIR}/external/cddm.cmake)

option(REPROC++ "Build reproc++.")

add_subdirectory(reproc)

if(REPROC++)
  enable_language(CXX)
  add_subdirectory(reproc++)
endif()
