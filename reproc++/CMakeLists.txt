reproc_add_library(reproc++ CXX 11)

target_link_libraries(reproc++ PRIVATE
  reproc
  $<$<BOOL:${REPROC_MULTITHREADED}>:Threads::Threads>
)

target_sources(reproc++ PRIVATE
  src/error.cpp
  src/reproc.cpp
  src/sink.cpp
)

if(REPROC_EXAMPLES)
  reproc_add_example(cmake-help CXX 11 reproc++)

  if(Threads_FOUND)
    reproc_add_example(background CXX 11 reproc++ Threads::Threads)
    reproc_add_example(forward CXX 11 reproc++ Threads::Threads)
  else()
    message(STATUS "Threads package not found. Not building threaded reproc++ examples.")
  endif()
endif()
