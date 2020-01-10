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
  reproc_add_example(reprocxx cmake-help CXX)
  reproc_add_example(reprocxx forward CXX)

  if(REPROC_MULTITHREADED)
    reproc_add_example(reprocxx background CXX Threads::Threads)
  endif()
endif()
