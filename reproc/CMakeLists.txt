reproc_add_library(reproc C)

target_link_libraries(reproc PRIVATE Threads::Threads)

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
  src/reproc.c
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
    set_tests_properties(${TARGET} PROPERTIES FIXTURES_REQUIRED build)

    add_executable(reproc-resource-${TARGET} resources/${TARGET}.c)

    reproc_add_common(
      reproc-resource-${TARGET} C
      OUTPUT_DIRECTORY resources
      OUTPUT_NAME ${TARGET}
    )

    # Make sure the test resource is available when running the test.
    add_dependencies(reproc-test-${TARGET} reproc-resource-${TARGET})
  endfunction()

  reproc_add_test(argv)
  reproc_add_test(environment)
  reproc_add_test(io)
  reproc_add_test(overflow)
  reproc_add_test(stop)
  reproc_add_test(working-directory)
endif()

if(REPROC_EXAMPLES)
  reproc_add_example(drain C reproc)
  reproc_add_example(git-status C reproc)
  reproc_add_example(inherit C reproc)
endif()
