reproc_add_library(reproc C)

if(REPROC_MULTITHREADED)
  target_compile_definitions(reproc PRIVATE REPROC_MULTITHREADED)
  target_link_libraries(reproc PRIVATE Threads::Threads)
endif()

if(WIN32)
  set(REPROC_PLATFORM windows)
  target_compile_definitions(reproc PRIVATE WIN32_LEAN_AND_MEAN)
  target_link_libraries(reproc PRIVATE ws2_32)
else()
  set(REPROC_PLATFORM posix)
  target_link_libraries(reproc PRIVATE util)
endif()

target_sources(reproc PRIVATE
  src/${REPROC_PLATFORM}/clock.c
  src/${REPROC_PLATFORM}/error.c
  src/${REPROC_PLATFORM}/handle.c
  src/${REPROC_PLATFORM}/init.c
  src/${REPROC_PLATFORM}/pipe.c
  src/${REPROC_PLATFORM}/process.c
  src/${REPROC_PLATFORM}/redirect.c
  src/reproc.c
  src/sink.c
  src/run.c
)

if(REPROC_TEST)
  function(reproc_add_test TARGET)
    add_executable(reproc-test-${TARGET} test/${TARGET}.c)

    reproc_add_common(
      reproc-test-${TARGET} C
      OUTPUT_DIRECTORY test
      OUTPUT_NAME ${TARGET}
    )

    target_link_libraries(reproc-test-${TARGET} PRIVATE reproc)
    target_compile_definitions(reproc-test-${TARGET} PRIVATE
      RESOURCE_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}/resources"
    )

    add_test(NAME ${TARGET} COMMAND reproc-test-${TARGET})

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/${TARGET}.c)
      add_executable(reproc-resource-${TARGET} resources/${TARGET}.c)

      reproc_add_common(
        reproc-resource-${TARGET} C
        OUTPUT_DIRECTORY resources
        OUTPUT_NAME ${TARGET}
      )

      # Make sure the test resource is available when running the test.
      add_dependencies(reproc-test-${TARGET} reproc-resource-${TARGET})
    endif()
  endfunction()

  reproc_add_test(argv)
  reproc_add_test(environment)
  reproc_add_test(io)
  reproc_add_test(overflow)
  reproc_add_test(stop)
  reproc_add_test(working-directory)

  if(UNIX)
    reproc_add_test(fork)
  endif()
endif()

if(REPROC_EXAMPLES)
  reproc_add_example(reproc drain C)
  reproc_add_example(reproc git-status C)
  reproc_add_example(reproc inherit C)
  reproc_add_example(reproc run C)
endif()
