include(FetchContent)

FetchContent_Declare(
  doctest
  URL https://github.com/onqtam/doctest/archive/2.3.3.tar.gz
)

if(REPROC_TEST)
  option(DOCTEST_WITH_TESTS "" OFF)
  option(DOCTEST_NO_INSTALL "" ON)

  reproc_MakeAvailable(doctest)
endif()

if(REPROC_MULTITHREADED AND NOT TARGET Threads::Threads)
  if(NOT DEFINED THREADS_PREFER_PTHREAD_FLAG)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
  endif()

  find_package(Threads REQUIRED)
endif()
