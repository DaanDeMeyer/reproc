reproc_add_library(reprocxx CXX 11)

target_link_libraries(reprocxx PRIVATE reproc Threads::Threads)

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
  reproc_add_example(cmake-help CXX 11 reprocxx)
  reproc_add_example(forward CXX 11 reprocxx)
  reproc_add_example(background CXX 11 reprocxx Threads::Threads)
endif()
