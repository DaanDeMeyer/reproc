reproc_add_library(reproc++ CXX)

target_link_libraries(reproc++ PRIVATE
  reproc
  $<$<BOOL:${REPROC_MULTITHREADED}>:Threads::Threads>
)

target_sources(
  reproc++
  PRIVATE
    src/reproc.cpp
  INTERFACE
    # We manually propagate reproc's object files until CMake adds support for
    # doing it automatically.
    $<$<BOOL:${REPROC_OBJECT_LIBRARIES}>:$<TARGET_OBJECTS:reproc>>
)

if(REPROC_EXAMPLES)
  reproc_add_example(reproc++ drain CXX)
  reproc_add_example(reproc++ forward CXX)
  reproc_add_example(reproc++ run CXX)

  if(REPROC_MULTITHREADED)
    reproc_add_example(reproc++ background CXX Threads::Threads)
  endif()
endif()
