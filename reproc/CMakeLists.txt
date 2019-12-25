reproc_add_library(reproc C 99)

if(REPROC_MULTITHREADED)
  target_compile_definitions(reproc PRIVATE REPROC_MULTITHREADED)
  target_link_libraries(reproc PRIVATE Threads::Threads)
endif()

target_sources(reproc PRIVATE src/reproc.c src/sink.c)

if(UNIX)
  set(REPROC_PLATFORM posix)
elseif(WIN32)
  set(REPROC_PLATFORM windows)
endif()

target_sources(reproc PRIVATE
  src/${REPROC_PLATFORM}/error.c
  src/${REPROC_PLATFORM}/handle.c
  src/${REPROC_PLATFORM}/pipe.c
  src/${REPROC_PLATFORM}/process.c
  src/${REPROC_PLATFORM}/redirect.c
)

target_compile_definitions(reproc PRIVATE
  $<$<PLATFORM_ID:Windows>:
    WIN32_LEAN_AND_MEAN
    # Needed for `WC_ERR_INVALID_CHARS` and `InitializeProcThreadAttributeList`.
    _WIN32_WINNT=_WIN32_WINNT_VISTA
    NTDDI_VERSION=NTDDI_VISTA
  >
  $<$<PLATFORM_ID:Linux>:
    # Needed for `pipe2` and `execvpe`.
    _GNU_SOURCE
  >
)

if(REPROC_TEST)
  target_sources(reproc-test PRIVATE
    test/argv.cpp
    test/environment.cpp
    test/io.cpp
    test/overflow.cpp
    test/stop.cpp
    test/working-directory.cpp
  )

  target_link_libraries(reproc-test PRIVATE reproc)

  function(reproc_add_test_helper TARGET)
    add_executable(reproc-${TARGET})

    reproc_add_common(reproc-${TARGET} CXX 11 resources)
    target_sources(reproc-${TARGET} PRIVATE resources/${TARGET}.cpp)
    set_target_properties(reproc-${TARGET} PROPERTIES OUTPUT_NAME ${TARGET})

    # Build the helper executable as part of reproc-test build.
    add_dependencies(reproc-test reproc-${TARGET})
  endfunction()

  # Helper binaries required by reproc's tests.
  reproc_add_test_helper(argv)
  reproc_add_test_helper(environment)
  reproc_add_test_helper(infinite)
  reproc_add_test_helper(io)
  reproc_add_test_helper(working-directory)
  reproc_add_test_helper(overflow)

  target_compile_definitions(reproc-test PRIVATE
    RESOURCE_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}/resources"
  )
endif()

if(REPROC_EXAMPLES)
  reproc_add_example(drain C 99 reproc)
  reproc_add_example(git-status C 99 reproc)
  reproc_add_example(inherit C 99 reproc)
endif()
