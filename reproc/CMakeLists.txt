reproc_add_library(reproc C 99)

if(REPROC_MULTITHREADED)
  target_compile_definitions(reproc PRIVATE REPROC_MULTITHREADED)
  target_link_libraries(reproc PRIVATE Threads::Threads)
endif()

target_sources(reproc PRIVATE src/common.c src/sink.c)

if(WIN32)
  target_sources(reproc PRIVATE
    src/windows/error.c
    src/windows/handle.c
    src/windows/pipe.c
    src/windows/process.c
    src/windows/reproc.c
  )
elseif(UNIX)
  target_sources(reproc PRIVATE
    src/posix/error.c
    src/posix/fd.c
    src/posix/path.c
    src/posix/pipe.c
    src/posix/process.c
    src/posix/reproc.c
  )
endif()

include(CheckSymbolExists)

if(WIN32)
  target_compile_definitions(reproc PRIVATE WIN32_LEAN_AND_MEAN)
endif()

if(MINGW)
  target_compile_definitions(reproc PRIVATE
    WINVER=0x0600 # `WC_ERR_INVALID_CHARS`
    _WIN32_WINNT=0x0600 # `InitializeProcThreadAttributeList`
  )
endif()

if(UNIX)
  # Check if `pipe2` is available.
  # Access to `pipe2` on Linux requires `_GNU_SOURCE` to be defined.

  if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  endif()

  check_symbol_exists(pipe2 unistd.h REPROC_PIPE2_FOUND)

  if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    list(REMOVE_AT CMAKE_REQUIRED_DEFINITIONS -1)
  endif()

  target_compile_definitions(reproc PRIVATE
    $<$<PLATFORM_ID:Linux>:_GNU_SOURCE> # Needed for `pipe2` and `execvpe`.
    $<$<BOOL:${REPROC_PIPE2_FOUND}>:REPROC_PIPE2_FOUND>
  )
endif()

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
  reproc_add_test_helper(noop)
  reproc_add_test_helper(overflow)

  target_compile_definitions(reproc-test PRIVATE
    RESOURCE_DIRECTORY="${CMAKE_CURRENT_BINARY_DIR}/resources"
  )
endif()

if(REPROC_EXAMPLES)
  reproc_add_example(drain C 99 reproc)
  reproc_add_example(git-status C 99 reproc)
endif()
