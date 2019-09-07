if(REPROC_TEST)
  if(NOT EXISTS ${PROJECT_BINARY_DIR}/doctest)
    file(
      DOWNLOAD
      https://raw.githubusercontent.com/onqtam/doctest/2.3.4/doctest/doctest.h
      ${PROJECT_BINARY_DIR}/doctest/doctest.h
    )
  endif()

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

  add_library(doctest OBJECT ${PROJECT_BINARY_DIR}/doctest/impl.cpp)
  target_compile_features(doctest PUBLIC cxx_std_11)
  target_include_directories(doctest PUBLIC ${PROJECT_BINARY_DIR}/doctest)
endif()

# Search for `Threads` module ourselves if the user has not already done so.
if(NOT DEFINED Threads_FOUND)
  # See https://cmake.org/cmake/help/v3.15/module/FindThreads.html
  if(NOT DEFINED THREADS_PREFER_PTHREAD_FLAG)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
  endif()

  find_package(Threads)
endif()

if(REPROC_MULTITHREADED AND NOT Threads_FOUND)
  message(FATAL_ERROR "`REPROC_MULTITHREADED` is enabled but the CMake `Threads` module was not found.")
endif()
