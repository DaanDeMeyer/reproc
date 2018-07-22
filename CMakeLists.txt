cmake_minimum_required(VERSION 3.1)
project(reproc VERSION 0.0.1 LANGUAGES C)

option(REPROC_BUILD_TESTS "Build tests" OFF)
option(REPROC_BUILD_EXAMPLES "Build examples. Enable REPROC_BUILD_CXX_WRAPPER \
to build C++ examples." OFF)
option(REPROC_BUILD_CXX_WRAPPER "Build C++ wrapper" OFF)
option(REPROC_CI "Add -Werror or equivalent to compiler and clang-tidy" OFF)
option(REPROC_TIDY "Run clang-tidy as part of the build \
(requires clang-tidy to be in the PATH)" OFF)
# Don't generate install target by default so we don't interfere with install
# targets when reproc is used via add_subdirectory
option(REPROC_INSTALL "Generate install target" OFF)
option(REPROC_FORMAT "Find clang-format and add target to run it on all source \
files")

### Helpers ###

function(reproc_set_c_standard TARGET VERSION)
  set_target_properties(${TARGET} PROPERTIES
    C_STANDARD ${VERSION}
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
  )
endfunction()

function(reproc_set_cxx_standard TARGET VERSION)
  set_target_properties(${TARGET} PROPERTIES
    CXX_STANDARD ${VERSION}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )
endfunction()

### clang-tidy ###

if (REPROC_RUN_CLANG_TIDY)
  cmake_minimum_required(VERSION 3.6)
  find_program(REPROC_CLANG_TIDY clang-tidy)

  if(REPROC_CLANG_TIDY)
    message(STATUS "clang-tidy found")
    if (REPROC_CI)
      set(REPROC_CLANG_TIDY ${REPROC_CLANG_TIDY} -warnings-as-errors=*)
    endif()
  else()
    message(WARNING "clang-tidy not found")
    # If clang-tidy is not found REPROC_CLANG_TIDY is not set to an empty value
    # so we have to override it
    set(REPROC_CLANG_TIDY "")
  endif()
else()
  # find_program puts its results in the CMake cache. When REPROC_RUN_CLANG_TIDY
  # is set to OFF in a subsequent invocation of CMake we have to override
  # REPROC_CLANG_TIDY to make sure it doesn't contain a cache value
  set(REPROC_CLANG_TIDY "")
endif()

### Common options ###

if(MSVC)
  set(REPROC_WARNINGS
    # /W3 is enabled by default
    $<$<BOOL:${REPROC_CI}>:/WX>
  )
else()
  set(REPROC_WARNINGS
    -Wall
    -Wextra
    -pedantic-errors
    -Wshadow
    -Wconversion
    $<$<BOOL:${REPROC_CI}>:-Werror>
    $<$<C_COMPILER_ID:Clang>:-fcolor-diagnostics>
    $<$<C_COMPILER_ID:GNU>:-fdiagnostics-color>
  )
endif()

### Library ###

add_library(reproc "")
add_library(reproc::reproc ALIAS reproc)

set_target_properties(reproc PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY lib
  ARCHIVE_OUTPUT_DIRECTORY lib
  LIBRARY_OUTPUT_DIRECTORY lib
  # Might not be set so surround with quotes so we always have a value
  C_CLANG_TIDY "${REPROC_CLANG_TIDY}"
)

reproc_set_c_standard(reproc 99)
target_compile_options(reproc PRIVATE ${REPROC_WARNINGS})

# Only use the headers from the repository when building. When installing we
# want to use the install location of the headers (e.g. /usr/include) as the
# include directory which is specified later in this file (Install section)
target_include_directories(reproc PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/c>
)

# Used to add __declspec(dllexport/dllimport) on Windows
target_compile_definitions(reproc
  PUBLIC $<$<BOOL:${BUILD_SHARED_LIBS}>:REPROC_SHARED>
  PRIVATE $<BUILD_INTERFACE:REPROC_BUILDING>
)

target_sources(reproc PRIVATE src/c/common.c)

if(WIN32)
  target_include_directories(reproc PRIVATE src/c/windows)
  target_sources(reproc PRIVATE
    src/c/windows/handle.c
    src/c/windows/pipe.c
    src/c/windows/process_utils.c
    src/c/windows/reproc.c
    src/c/windows/string_utils.c
  )
elseif(UNIX)
  target_include_directories(reproc PRIVATE src/c/posix)
  target_sources(reproc PRIVATE
    src/c/posix/constants.c
    src/c/posix/fork_exec_redirect.c
    src/c/posix/pipe.c
    src/c/posix/reproc.c
    src/c/posix/wait.c
  )
endif()

if(MSVC)
  # Check if STARTUPINFOEX and related functions are available
  include(CheckSymbolExists)
  check_symbol_exists(InitializeProcThreadAttributeList windows.h
                      REPROC_ATTRIBUTE_LIST_FOUND)

  target_compile_definitions(reproc PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    VC_EXTRALEAN
    NOMINMAX
    $<$<BOOL:${REPROC_ATTRIBUTE_LIST_FOUND}>:HAS_ATTRIBUTE_LIST>
  )
elseif(UNIX)
  # Check if pipe2 is available
  include(CheckSymbolExists)
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  check_symbol_exists(pipe2 unistd.h REPROC_PIPE2_FOUND)
  list(REMOVE_AT CMAKE_REQUIRED_DEFINITIONS -1)

  target_compile_definitions(reproc PRIVATE
    _GNU_SOURCE
    $<$<BOOL:${REPROC_PIPE2_FOUND}>:HAS_PIPE2>
  )
endif()

### C++ Wrapper ###

if(REPROC_BUILD_CXX_WRAPPER)
  enable_language(CXX)

  reproc_set_cxx_standard(reproc 11)
  set_target_properties(reproc PROPERTIES
    CXX_CLANG_TIDY "${REPROC_CLANG_TIDY}"
  )
  target_include_directories(reproc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cpp>
  )
  target_sources(reproc PRIVATE src/cpp/reproc.cpp src/cpp/error.cpp)
endif()

### Tests ###

if(REPROC_BUILD_TESTS)
  enable_language(CXX)

  ### Tests ###

  add_executable(reproc-test "")
  reproc_set_cxx_standard(reproc-test 11)

  set_target_properties(reproc-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY test
    CXX_CLANG_TIDY "${REPROC_CLANG_TIDY}"
  )

  target_link_libraries(reproc-test PRIVATE reproc)
  target_include_directories(reproc-test SYSTEM PRIVATE third-party/doctest)
  target_compile_options(reproc-test PRIVATE ${REPROC_WARNINGS})

  target_sources(reproc-test PRIVATE
    test/impl.cpp
    test/read-write.cpp
    test/stop.cpp
    test/working-directory.cpp
  )

  ### Helper programs ###

  function(reproc_add_test_helper TARGET)
    add_executable(${TARGET} test/res/${TARGET}.cpp)
    target_compile_options(${TARGET} PRIVATE ${REPROC_WARNINGS})
    set_target_properties(${TARGET} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY test/res
      CXX_CLANG_TIDY "${REPROC_CLANG_TIDY}"
    )
    set(REPROC_TEST_HELPERS ${REPROC_TEST_HELPERS} ${TARGET} PARENT_SCOPE)
    string(TOUPPER ${TARGET} TARGET_UPPER_CASE)
    # Pass paths as compile time parameters so tests can be run from any
    # directory without hardcoding the path in the tests which differs for each
    # user.
    target_compile_definitions(reproc-test PRIVATE
      ${TARGET_UPPER_CASE}_HELPER="$<TARGET_FILE:${TARGET}>"
    )
  endfunction()

  # Use _ because macro names in C++ can't contain -
  reproc_add_test_helper(reproc_stdout)
  reproc_add_test_helper(reproc_stderr)
  reproc_add_test_helper(reproc_infinite)
  reproc_add_test_helper(reproc_noop)

  add_dependencies(reproc-test ${REPROC_TEST_HELPERS})

  target_compile_definitions(reproc-test PRIVATE
    REPROC_NOOP_DIR="$<TARGET_FILE_DIR:reproc_noop>"
  )

  add_custom_target(
    reproc-run-tests
    COMMAND $<TARGET_FILE:reproc-test> --force-colors=true
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  add_dependencies(reproc-run-tests reproc-test)

endif()

### Examples ###

if(REPROC_BUILD_EXAMPLES)

  function(reproc_add_c_example TARGET)
    add_executable(${TARGET} example/${TARGET}.c)
    reproc_set_c_standard(${TARGET} 99)
    target_link_libraries(${TARGET} PRIVATE reproc)
    target_compile_options(${TARGET} PRIVATE ${REPROC_WARNINGS})
    set_target_properties(${TARGET} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY example
      C_CLANG_TIDY "${REPROC_CLANG_TIDY}"
    )
  endfunction()

  function(reproc_add_cxx_example TARGET)
    add_executable(${TARGET} example/${TARGET}.cpp)
    reproc_set_cxx_standard(${TARGET} 11)
    target_link_libraries(${TARGET} PRIVATE reproc)
    target_compile_options(${TARGET} PRIVATE ${REPROC_WARNINGS})
    set_target_properties(${TARGET} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY example
      CXX_CLANG_TIDY "${REPROC_CLANG_TIDY}"
    )
  endfunction()

  reproc_add_c_example(reproc-git-status)

  if(REPROC_BUILD_CXX_WRAPPER)
    reproc_add_cxx_example(reproc-cmake-help)
    reproc_add_cxx_example(reproc-forward)
  endif()
endif()

### Install ###

# Adapted from https://codingnest.com/basic-cmake-part-2/

if(REPROC_INSTALL)

  include(GNUInstallDirs)
  set(REPROC_INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/reproc)

  install(
    TARGETS reproc
    EXPORT reproc-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

  target_include_directories(reproc PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  install(
    EXPORT reproc-targets
    FILE reproc-targets.cmake
    NAMESPACE reproc::
    DESTINATION ${REPROC_INSTALL_CONFIGDIR}
  )

  install(
    DIRECTORY include/c/reproc
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  if(REPROC_BUILD_CXX_WRAPPER)
    install(
      DIRECTORY include/cpp/reproc
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
  endif()

  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
      ${CMAKE_CURRENT_BINARY_DIR}/reproc-config-version.cmake
      VERSION ${PROJECT_VERSION}
      COMPATIBILITY AnyNewerVersion
  )

  configure_package_config_file(
      ${CMAKE_CURRENT_LIST_DIR}/cmake/reproc-config.cmake.in
      ${CMAKE_CURRENT_BINARY_DIR}/reproc-config.cmake
      INSTALL_DESTINATION ${REPROC_INSTALL_CONFIGDIR}}
  )

  install(
      FILES
        ${CMAKE_CURRENT_BINARY_DIR}/reproc-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/reproc-config-version.cmake
      DESTINATION ${REPROC_INSTALL_CONFIGDIR}
  )

endif()

### clang-format ###

if (REPROC_FORMAT)
find_program(REPROC_CLANG_FORMAT clang-format)
if(REPROC_CLANG_FORMAT)
  message(STATUS "clang-format found. Adding reproc-format target")
  file(GLOB_RECURSE REPROC_SOURCES
    include/*.[h,c,hpp,cpp]
    src/*.[h,c,hpp,cpp]
    test/*.[h,c,hpp,cpp]
    example/*.[h,c,hpp,cpp]
  )

  add_custom_target(
    reproc-format
    COMMAND ${REPROC_CLANG_FORMAT} -i ${REPROC_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
endif()
