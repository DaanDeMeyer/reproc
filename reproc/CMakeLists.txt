if(NOT DEFINED THREADS_PREFER_PTHREAD_FLAG)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()

# reproc's pkg-config file requires `CMAKE_THREAD_LIBS_INIT` to be defined so we
# find the Threads package (which initializes `CMAKE_THREAD_LIBS_INIT`) before
# calling `cddm_add_library` (which generates the pkg-config file).
find_package(Threads REQUIRED)

cddm_add_library(reproc C 99)

target_link_libraries(reproc PRIVATE Threads::Threads)

target_sources(reproc PRIVATE
  src/common/error.c
  src/common/infinite.c
  src/common/reproc.c
)

if(WIN32)
  target_sources(reproc PRIVATE
    src/windows/error.c
    src/windows/handle.c
    src/windows/pipe.c
    src/windows/process.c
    src/windows/reproc.c
    src/windows/string_utils.c
  )
elseif(UNIX)
  target_sources(reproc PRIVATE
    src/posix/error.c
    src/posix/fd.c
    src/posix/pipe.c
    src/posix/process.c
    src/posix/reproc.c
  )
endif()

include(CheckSymbolExists)

if(MSVC)
  # Check if `STARTUPINFOEX` and functions related to it are available.
  check_symbol_exists(InitializeProcThreadAttributeList windows.h
                      REPROC_ATTRIBUTE_LIST_FOUND)

  target_compile_definitions(reproc PRIVATE
    _CRT_SECURE_NO_WARNINGS
    $<$<BOOL:${REPROC_ATTRIBUTE_LIST_FOUND}>:HAVE_ATTRIBUTE_LIST>
    # Remove unneeded stuff from windows.h.
    WIN32_LEAN_AND_MEAN
    VC_EXTRALEAN
    NOMINMAX
  )
elseif(UNIX)
  # Check if `pipe2` is available.
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  check_symbol_exists(pipe2 unistd.h REPROC_PIPE2_FOUND)
  list(REMOVE_AT CMAKE_REQUIRED_DEFINITIONS -1)

  target_compile_definitions(reproc PRIVATE
    _GNU_SOURCE # Needed for `pipe2` and `kill`.
    $<$<BOOL:${REPROC_PIPE2_FOUND}>:HAVE_PIPE2>
  )
endif()

if(REPROC_TESTS)
  target_link_libraries(reproc-tests PRIVATE reproc)

  target_sources(reproc-tests PRIVATE
    tests/read-write.cpp
    tests/stop.cpp
    tests/working-directory.cpp
  )

  function(reproc_add_test_helper TARGET)
    add_executable(reproc-${TARGET})

    cddm_add_common(reproc-${TARGET} CXX 11 resources)
    target_sources(reproc-${TARGET} PRIVATE resources/${TARGET}.cpp)
    set_target_properties(reproc-${TARGET} PROPERTIES OUTPUT_NAME ${TARGET})

    # Build the helper executable as part of reproc-tests build.
    add_dependencies(reproc-tests reproc-${TARGET})
  endfunction()

  # Helper binaries required by reproc's tests.
  reproc_add_test_helper(stdout)
  reproc_add_test_helper(stderr)
  reproc_add_test_helper(infinite)
  reproc_add_test_helper(noop)
endif()

if(REPROC_EXAMPLES)
  function(reproc_add_example TARGET)
    add_executable(reproc-${TARGET})

    cddm_add_common(reproc-${TARGET} C 99 examples)
    target_sources(reproc-${TARGET} PRIVATE examples/${TARGET}.c)
    target_link_libraries(reproc-${TARGET} PRIVATE reproc)
    set_target_properties(reproc-${TARGET} PROPERTIES OUTPUT_NAME ${TARGET})

    if(REPROC_SANITIZERS)
      set_target_properties(reproc-${TARGET} PROPERTIES
        # Hack to avoid UBSan undefined reference errors.
        LINKER_LANGUAGE CXX
      )
    endif()
  endfunction()

  reproc_add_example(git-status C)
  reproc_add_example(drain C)
endif()
