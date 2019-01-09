# 3.1 is needed for target_sources.
cmake_minimum_required(VERSION 3.1...3.13)

# Use updated policies on newer versions of CMake.
if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(reproc VERSION 3.1.3 LANGUAGES C)

option(REPROC++ "Build reproc++.")
option(REPROC_TESTS "Build tests.")
option(REPROC_EXAMPLES "Build examples.")

if(REPROC++ OR REPROC_TESTS)
  enable_language(CXX)
endif()

include(${PROJECT_SOURCE_DIR}/external/cddm.cmake)

add_subdirectory(reproc)
if(REPROC++)
  add_subdirectory(reproc++)
endif()
