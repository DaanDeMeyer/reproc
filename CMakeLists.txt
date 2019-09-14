# 3.13 is needed for `target_link_options`.
cmake_minimum_required(VERSION 3.13...3.15)

project(reproc
  VERSION 9.0.0
  DESCRIPTION "Cross-platform C99/C++11 process library"
  HOMEPAGE_URL "https://github.com/DaanDeMeyer/reproc"
  LANGUAGES C
)

option(REPROC++ "Build reproc++.")
option(REPROC_TEST "Build tests.")
option(REPROC_EXAMPLES "Build examples.")

if(UNIX)
  option(REPROC_MULTITHREADED "Use `pthread_sigmask` instead of `sigprocmask`." ON)
endif()

# Enable languages before configuring all enabled languages in reproc.cmake.
if(REPROC++ OR REPROC_TEST)
  enable_language(CXX)
endif()

include(cmake/reproc.cmake)
include(cmake/external.cmake)

if(REPROC_TEST)
  # reproc uses the doctest testing framework. All tests are grouped inside a
  # single executable. Use `target_sources(reproc-test PRIVATE <file>)` to add
  # new tests. Tests are executed by building the `test` target or running the
  # tests executable from within the build directory.

  # Generate the doctest implementation file.
  file(
    GENERATE
    OUTPUT ${PROJECT_BINARY_DIR}/doctest/impl.cpp
    CONTENT
      "
      #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN\n
      #define DOCTEST_CONFIG_NO_POSIX_SIGNALS\n
      #include <doctest.h>
      "
  )

  add_executable(reproc-test ${PROJECT_BINARY_DIR}/doctest/impl.cpp)

  reproc_add_common(reproc-test CXX 11 "")
  target_link_libraries(reproc-test PRIVATE doctest)
  set_target_properties(reproc-test PROPERTIES OUTPUT_NAME tests)

  if(NOT REPROC_IS_SUBDIRECTORY)
    add_custom_target(
      test
      COMMAND ./tests -force-colors
      DEPENDS reproc-test
      # Tests include relative paths to some external test programs and can
      # only be executed from inside the build folder as a result.
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
  endif()
endif()

add_subdirectory(reproc)
if(REPROC++)
  add_subdirectory(reproc++)
endif()
