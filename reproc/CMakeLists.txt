# reproc_add_library generates the pkg-config file immediately. reproc's
# pkg-config file requires CMAKE_THREAD_LIBS_INIT to be defined so we find
# threads before calling reproc_add_library.
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
  # Check if STARTUPINFOEX and related functions are available.
  check_symbol_exists(InitializeProcThreadAttributeList windows.h
                      REPROC_ATTRIBUTE_LIST_FOUND)

  target_compile_definitions(reproc PRIVATE
    _CRT_SECURE_NO_WARNINGS
    # Remove unneeded stuff from windows.h.
    WIN32_LEAN_AND_MEAN
    VC_EXTRALEAN
    NOMINMAX
    $<$<BOOL:${REPROC_ATTRIBUTE_LIST_FOUND}>:HAVE_ATTRIBUTE_LIST>
  )
elseif(UNIX)
  # Check if pipe2 is available.
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  check_symbol_exists(pipe2 unistd.h REPROC_PIPE2_FOUND)
  list(REMOVE_AT CMAKE_REQUIRED_DEFINITIONS -1)

  target_compile_definitions(reproc PRIVATE
    _GNU_SOURCE # Needed for pipe2 and kill.
    $<$<BOOL:${REPROC_PIPE2_FOUND}>:HAVE_PIPE2>
  )
endif()

if(REPROC_TESTS)
  add_executable(reproc-tests "")
  cddm_add_common(reproc-tests CXX 11 tests)

  target_link_libraries(reproc-tests PRIVATE reproc)
  target_include_directories(reproc-tests SYSTEM PRIVATE
                             ${PROJECT_SOURCE_DIR}/external/doctest)
  set_target_properties(reproc-tests PROPERTIES OUTPUT_NAME tests)

  target_sources(reproc-tests PRIVATE
    tests/impl.cpp
    tests/read-write.cpp
    tests/stop.cpp
    tests/working-directory.cpp
  )

  ### Helper programs ###

  function(reproc_add_test_helper TARGET)
    add_executable(reproc-${TARGET} tests/resources/${TARGET}.cpp)
    cddm_add_common(reproc-${TARGET} CXX 11 tests/resources)
    set_target_properties(reproc-tests PROPERTIES OUTPUT_NAME ${TARGET})

    string(TOUPPER ${TARGET} TARGET_UPPER_CASE)
    # Pass paths as compile time parameters so tests can be run from any
    # directory without hardcoding the path in the tests which differs for each
    # user.
    target_compile_definitions(reproc-tests PRIVATE
      ${TARGET_UPPER_CASE}_PATH="$<TARGET_FILE:reproc-${TARGET}>"
      ${TARGET_UPPER_CASE}_DIR="$<TARGET_FILE_DIR:reproc-${TARGET}>"
    )
    add_dependencies(reproc-tests reproc-${TARGET})
  endfunction()

  reproc_add_test_helper(stdout)
  reproc_add_test_helper(stderr)
  reproc_add_test_helper(infinite)
  reproc_add_test_helper(noop)

  add_custom_target(
    reproc-run-tests
    COMMAND $<TARGET_FILE:reproc-tests> --force-colors=true
  )

  add_dependencies(reproc-run-tests reproc-tests)
endif()

if(REPROC_EXAMPLES)
  function(reproc_add_example TARGET)
    add_executable(reproc-${TARGET} "")
    cddm_add_common(reproc-${TARGET} C 99 examples)

    target_link_libraries(reproc-${TARGET} PRIVATE reproc)
    target_sources(reproc-${TARGET} PRIVATE examples/${TARGET}.c)
    set_target_properties(reproc-${TARGET} PROPERTIES OUTPUT_NAME ${TARGET})

    if(REPROC_SANITIZERS)
      set_target_properties(reproc-${TARGET} PROPERTIES
        # Hack to avoid UBSAN undefined reference errors.
        LINKER_LANGUAGE CXX
      )
    endif()
  endfunction()

  reproc_add_example(git-status C)
endif()
