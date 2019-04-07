# 3.13 is needed for to allow using `target_link_libraries` to modify targets
# created outside the current directory.
cmake_minimum_required(VERSION 3.13...3.14)

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
  add_subdirectory(external/doctest)

  # reproc uses the doctest testing framework. All tests are grouped inside a
  # single executable. Use `target_sources(reproc-tests PRIVATE <file>)` to add
  # new tests. Tests are executed by running the build/tests executable.
  add_executable(reproc-tests)

  cddm_add_common(reproc-tests CXX 11 "")
  set_target_properties(reproc-tests PROPERTIES OUTPUT_NAME tests)
  target_link_libraries(reproc-tests PRIVATE doctest)

  include(CTest)
  add_test(NAME reproc COMMAND reproc-tests --force-colors=true)
endif()

add_subdirectory(reproc)
if(REPROC++)
  add_subdirectory(reproc++)
endif()
