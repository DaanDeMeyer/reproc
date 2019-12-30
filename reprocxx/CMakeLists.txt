reproc_add_library(reprocxx CXX)

target_link_libraries(reprocxx PRIVATE
  reproc
  $<$<BOOL:${REPROC_MULTITHREADED}>:Threads::Threads>
)

target_sources(
  reprocxx
  PRIVATE
    src/reproc.cpp
  INTERFACE
    # We manually propagate reproc's object files until CMake adds support for
    # doing it automatically.
    $<$<BOOL:${REPROC_OBJECT_LIBRARIES}>:$<TARGET_OBJECTS:reproc>>
)

if(REPROC_EXAMPLES)
  reproc_add_example(cmake-help CXX reprocxx)
  reproc_add_example(forward CXX reprocxx)

  if(REPROC_MULTITHREADED)
    reproc_add_example(background CXX reprocxx Threads::Threads)
  endif()
endif()
