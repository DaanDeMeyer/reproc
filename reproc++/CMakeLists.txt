enable_language(CXX)

cddm_add_library(reproc++ CXX 11)

target_link_libraries(reproc++ PRIVATE reproc::reproc)
target_sources(reproc++ PRIVATE
  src/reproc.cpp
  src/error.cpp
  src/sink.cpp
)

if(REPROC_EXAMPLES)
  function(reprocxx_add_example TARGET)
    add_executable(reproc++-${TARGET} "")
    cddm_add_common(reproc++-${TARGET} CXX 11 examples)

    target_link_libraries(reproc++-${TARGET} PRIVATE reproc::reproc++)
    target_sources(reproc++-${TARGET} PRIVATE examples/${TARGET}.cpp)
    set_target_properties(reproc++-${TARGET} PROPERTIES OUTPUT_NAME ${TARGET})
  endfunction()

  reprocxx_add_example(cmake-help)

  find_package(Threads)
  if(Threads_FOUND)
    reprocxx_add_example(forward CXX)
    target_link_libraries(reproc++-forward PRIVATE Threads::Threads)

    reprocxx_add_example(background CXX)
    target_link_libraries(reproc++-background PRIVATE Threads::Threads)
  else()
    message(WARNING "Threads package not found. Not building all reproc++ \
    examples.")
  endif()
endif()
