reproc_add_library(reproc++ CXX 11)

target_link_libraries(reproc++ PRIVATE
  reproc
  $<$<BOOL:${REPROC_MULTITHREADED}>:Threads::Threads>
)

target_sources(
  reproc++
  PRIVATE
    src/detail/array.cpp
    src/error.cpp
    src/reproc.cpp
    src/sink.cpp
  INTERFACE
    # We manually propagate reproc's object files until CMake adds support for
    # doing it automatically.
    $<$<BOOL:${REPROC_OBJECT_LIBRARIES}>:$<TARGET_OBJECTS:reproc>>
)

if(REPROC_EXAMPLES)
  reproc_add_example(cmake-help CXX 11 reproc++)
  reproc_add_example(forward CXX 11 reproc++)

  if(REPROC_MULTITHREADED)
    reproc_add_example(background CXX 11 reproc++ Threads::Threads)
  endif()
endif()
