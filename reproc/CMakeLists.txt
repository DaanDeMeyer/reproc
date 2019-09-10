reproc_add_library(reproc C 99)

if(REPROC_MULTITHREADED)
  target_link_libraries(reproc PRIVATE Threads::Threads)
endif()

target_sources(reproc PRIVATE src/common.c)

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
  # Check if `STARTUPINFOEX` and functions related to it are available.
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_WIN32_WINNT=0x0600 -DWINVER=0x0600)
  check_symbol_exists(InitializeProcThreadAttributeList Windows.h
                      REPROC_ATTRIBUTE_LIST_FOUND)
  list(REMOVE_AT CMAKE_REQUIRED_DEFINITIONS -1)

  target_compile_definitions(reproc PRIVATE
    _CRT_SECURE_NO_WARNINGS
    $<$<BOOL:${REPROC_ATTRIBUTE_LIST_FOUND}>:ATTRIBUTE_LIST_FOUND>
    # Remove unneeded stuff from windows.h.
    WIN32_LEAN_AND_MEAN
    VC_EXTRALEAN
    NOMINMAX

    $<$<BOOL:${MINGW}>:
      WINVER=0x0600
      _WIN32_WINNT=0x0600
    >
  )
else()
  # Check if `pipe2` is available.
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  check_symbol_exists(pipe2 unistd.h REPROC_PIPE2_FOUND)
  list(REMOVE_AT CMAKE_REQUIRED_DEFINITIONS -1)

  target_compile_definitions(reproc PRIVATE
    _GNU_SOURCE # Needed for `pipe2` and `kill`.
    $<$<BOOL:${REPROC_PIPE2_FOUND}>:PIPE2_FOUND>
  )
endif()

if(REPROC_TEST)
  target_sources(reproc-test PRIVATE
    test/argv.cpp
    test/environment.cpp
    test/io.cpp
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
  reproc_add_test_helper(stdout)
  reproc_add_test_helper(stderr)
  reproc_add_test_helper(infinite)
  reproc_add_test_helper(noop)
endif()

if(REPROC_EXAMPLES)
  reproc_add_example(git-status C 99 reproc)
  reproc_add_example(drain C 99 reproc)
endif()
