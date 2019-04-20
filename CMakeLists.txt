# 3.13 is needed for to allow using `target_link_libraries` to modify targets
# created outside the current directory.
cmake_minimum_required(VERSION 3.13...3.14)

project(reproc
  VERSION 6.0.0
  DESCRIPTION "Cross-platform C99/C++11 process library"
  HOMEPAGE_URL "https://github.com/DaanDeMeyer/reproc"
  LANGUAGES C
)

option(REPROC++ "Build reproc++.")
option(REPROC_TEST "Build tests.")
option(REPROC_EXAMPLES "Build examples.")

# Enable languages before including cddm as required by cddm.
if(REPROC++ OR REPROC_TEST)
  enable_language(CXX)
endif()

include(external/cddm/cddm.cmake)

if(REPROC_TEST)
  # reproc uses the doctest testing framework. All tests are grouped inside a
  # single executable. Use `target_sources(reproc-test PRIVATE <file>)` to add
  # new tests. Tests are executed by running the build/test executable.
  add_executable(reproc-test)

  cddm_add_common(reproc-test CXX 11 "")
  target_include_directories(reproc-test PRIVATE external/doctest)
  set_target_properties(reproc-test PROPERTIES OUTPUT_NAME test)
  # We change the working directory to the build directory before executing the
  # tests so tests can reliably find any required resources.
  target_compile_definitions(reproc-test PRIVATE
    DOCTEST_WORKING_DIRECTORY="${PROJECT_BINARY_DIR}"
  )

  target_sources(reproc-test PRIVATE external/doctest/main.cpp)
endif()

add_subdirectory(reproc)
if(REPROC++)
  add_subdirectory(reproc++)
endif()
