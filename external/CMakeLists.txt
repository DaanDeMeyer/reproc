if(REPROC_TEST)
  if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/external/doctest)
    file(
      DOWNLOAD
      https://raw.githubusercontent.com/onqtam/doctest/2.3.3/doctest/doctest.h
      ${CMAKE_CURRENT_BINARY_DIR}/external/doctest/doctest.h
    )
  endif()

  add_library(doctest INTERFACE IMPORTED GLOBAL)
  target_compile_features(doctest INTERFACE cxx_std_11)
  target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/external/doctest)
endif()

if(REPROC_MULTITHREADED AND NOT TARGET Threads::Threads)
  if(NOT DEFINED THREADS_PREFER_PTHREAD_FLAG)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
  endif()

  find_package(Threads REQUIRED)
endif()
