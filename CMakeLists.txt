# 3.1 is needed for target_sources.
cmake_minimum_required(VERSION 3.12...3.14)

project(reproc
  VERSION 5.0.0
  DESCRIPTION "Cross-platform C99/C++11 process library"
  HOMEPAGE_URL "https://github.com/DaanDeMeyer/reproc"
  LANGUAGES C
)

option(REPROC++ "Build reproc++.")
option(REPROC_TESTS "Build tests.")
option(REPROC_EXAMPLES "Build examples.")

# Enable languages before including cddm as required by cddm.
if(REPROC++ OR REPROC_TESTS)
  enable_language(CXX)
endif()

include(external/cddm/cddm.cmake)

if(REPROC_TESTS)
  # reproc uses the doctest testing framework. All tests are grouped inside a
  # single executable. Use `target_sources(reproc-tests PRIVATE <file>)` to add
  # new tests. Tests are executed by running the build/tests executable.
  add_executable(reproc-tests external/doctest/main.cpp)

  cddm_add_common(reproc-tests CXX 11 "")
  target_include_directories(reproc-tests PRIVATE external/doctest)
  set_target_properties(reproc-tests PROPERTIES OUTPUT_NAME tests)

  # We change the working directory to the build directory before executing the
  # tests so tests can reliably find any required resources.
  set_property(
    SOURCE external/doctest/main.cpp APPEND
    PROPERTY COMPILE_DEFINITIONS
      DOCTEST_WORKING_DIRECTORY="${PROJECT_BINARY_DIR}"
  )

  include(CTest)
  add_test(NAME reproc COMMAND reproc-tests --force-colors=true)
endif()

add_subdirectory(reproc)
if(REPROC++)
  add_subdirectory(reproc++)
endif()
