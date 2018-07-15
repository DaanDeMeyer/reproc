cmake_minimum_required(VERSION 3.1)
project(process-lib VERSION 0.0.1 LANGUAGES C)

option(PROCESS_LIB_BUILD_TESTS "Build tests" OFF)
option(PROCESS_LIB_BUILD_EXAMPLES "Build examples" OFF)
option(PROCESS_LIB_BUILD_CPP_WRAPPER "Build C++ wrapper" OFF)
option(PROCESS_LIB_CI "Add -Werror or equivalent to compiler and clang-tidy" OFF)
option(PROCESS_LIB_RUN_CLANG_TIDY "Run clang-tidy as part of the build \
(requires clang-tidy to be in the PATH)" OFF)

### Helpers ###

function(process_lib_set_c_standard TARGET VERSION)
  set_target_properties(${TARGET} PROPERTIES
    C_STANDARD ${VERSION}
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
  )
endfunction()

function(process_lib_set_cxx_standard TARGET VERSION)
  set_target_properties(${TARGET} PROPERTIES
    CXX_STANDARD ${VERSION}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )
endfunction()

### clang-tidy ###

if (PROCESS_LIB_RUN_CLANG_TIDY)
  cmake_minimum_required(VERSION 3.6)
  find_program(PROCESS_LIB_CLANG_TIDY clang-tidy)

  if(PROCESS_LIB_CLANG_TIDY AND PROCESS_LIB_CI)
    set(PROCESS_LIB_CLANG_TIDY ${PROCESS_LIB_CLANG_TIDY} -warnings-as-errors=*)
  endif()

  if(PROCESS_LIB_CLANG_TIDY)
    message(STATUS "clang-tidy found")
  else()
    set(PROCESS_LIB_CLANG_TIDY "")
    message(WARNING "clang-tidy not found")
  endif()
endif()

### Common Warnings ###

if(MSVC)
  set(PROCESS_LIB_WARNINGS
    /W3
    $<$<BOOL:${PROCESS_LIB_CI}>:/WX>
  )
else()
  set(PROCESS_LIB_WARNINGS
    -Wall
    -Wextra
    -pedantic-errors
    -Wshadow
    $<$<BOOL:${PROCESS_LIB_CI}>:-Werror>
  )
endif()

### Library ###

add_library(process-lib "")
add_library(process-lib::process-lib ALIAS process-lib)

set_target_properties(process-lib PROPERTIES
  # Make sure we get libprocess.so (Unix) and process.lib (Windows)
  OUTPUT_NAME process
  ARCHIVE_OUTPUT_DIRECTORY lib
  LIBRARY_OUTPUT_DIRECTORY lib
  # Might not be set so surround with quotes so we always have a value
  C_CLANG_TIDY "${PROCESS_LIB_CLANG_TIDY}"
)
process_lib_set_c_standard(process-lib 99)

target_compile_options(process-lib PRIVATE ${PROCESS_LIB_WARNINGS})

# Only use the headers from the repository when building. When installing we
# want to use the install location of the headers (e.g. /usr/include) as the
# include directory
target_include_directories(process-lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/c>
)

target_sources(process-lib PRIVATE src/c/common.c)

if(WIN32)
  target_include_directories(process-lib PRIVATE src/c/windows)
  target_sources(process-lib PRIVATE
    src/c/windows/handle.c
    src/c/windows/pipe.c
    src/c/windows/process_utils.c
    src/c/windows/process.c
    src/c/windows/string_utils.c
  )
elseif(UNIX)
  target_include_directories(process-lib PRIVATE src/c/posix)
  target_sources(process-lib PRIVATE
    src/c/posix/constants.c
    src/c/posix/fork_exec_redirect.c
    src/c/posix/pipe.c
    src/c/posix/process.c
    src/c/posix/wait.c
  )
endif()

if(MSVC)
  # Check if STARTUPINFOEX and related functions are available
  include(CheckSymbolExists)
  check_symbol_exists(InitializeProcThreadAttributeList windows.h
                      PROCESS_LIB_ATTRIBUTE_LIST_FOUND)

  target_compile_definitions(process-lib PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    VC_EXTRALEAN
    NOMINMAX
    $<$<BOOL:${PROCESS_LIB_ATTRIBUTE_LIST_FOUND}>:HAS_ATTRIBUTE_LIST>
  )
elseif(UNIX)
  # Check if pipe2 is available
  include(CheckSymbolExists)
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  check_symbol_exists(pipe2 unistd.h PROCESS_LIB_PIPE2_FOUND)
  list(REMOVE_AT CMAKE_REQUIRED_DEFINITIONS -1)

  target_compile_definitions(process-lib PRIVATE
    _GNU_SOURCE
    $<$<BOOL:${PROCESS_LIB_PIPE2_FOUND}>:HAS_PIPE2>
  )
endif()

### C++ Wrapper ###

if(PROCESS_LIB_BUILD_CPP_WRAPPER)
  enable_language(CXX)

  process_lib_set_cxx_standard(process-lib 11)
  set_target_properties(process-lib PROPERTIES
    CXX_CLANG_TIDY "${PROCESS_LIB_CLANG_TIDY}"
  )
  target_include_directories(process-lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/cpp>
  )
  target_sources(process-lib PRIVATE src/cpp/process.cpp)
endif()

### Tests ###

if(PROCESS_LIB_BUILD_TESTS)

  ### Helper programs ###

  enable_language(CXX)

  add_executable(echo test/res/echo.cpp)
  target_compile_options(echo PRIVATE ${PROCESS_LIB_WARNINGS})

  add_executable(infinite test/res/infinite.cpp)
  target_compile_options(infinite PRIVATE ${PROCESS_LIB_WARNINGS})

  add_executable(noop test/res/noop.cpp)
  target_compile_options(noop PRIVATE ${PROCESS_LIB_WARNINGS})

  set_target_properties(echo infinite noop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY test/res
    CXX_CLANG_TIDY "${PROCESS_LIB_CLANG_TIDY}"
  )

  ### Tests ###

  add_executable(tests "")
  add_dependencies(tests echo infinite noop)
  process_lib_set_cxx_standard(tests 11)
  set_target_properties(tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY test
    CXX_CLANG_TIDY "${PROCESS_LIB_CLANG_TIDY}"
  )
  target_link_libraries(tests PRIVATE process-lib)
  target_include_directories(tests SYSTEM PRIVATE third-party/doctest)
  target_compile_options(tests PRIVATE ${PROCESS_LIB_WARNINGS})

  # Pass paths as compile time parameters so tests can be run from any
  # directory without hardcoding the path in the tests which differs for each
  # user.
  target_compile_definitions(tests PRIVATE
    ECHO_PATH="$<TARGET_FILE:echo>"
    INFINITE_PATH="$<TARGET_FILE:infinite>"
    NOOP_PATH="$<TARGET_FILE:noop>"
    NOOP_DIR="$<TARGET_FILE_DIR:noop>"
  )

  target_sources(tests PRIVATE
    test/impl.cpp
    test/read-write.cpp
    test/stop.cpp
    test/working-directory.cpp
  )
endif()

### Examples ###

if(PROCESS_LIB_BUILD_EXAMPLES)

  add_executable(cmake-help-c examples/cmake-help.c)
  process_lib_set_c_standard(cmake-help-c 99)
  target_link_libraries(cmake-help-c PRIVATE process-lib)
  target_compile_options(cmake-help-c PRIVATE ${PROCESS_LIB_WARNINGS})

  set_target_properties(cmake-help-c PROPERTIES
    C_CLANG_TIDY "${PROCESS_LIB_CLANG_TIDY}"
  )

  add_executable(cmake-help-cpp examples/cmake-help.cpp)
  process_lib_set_cxx_standard(cmake-help-cpp 11)
  target_link_libraries(cmake-help-cpp process-lib)
  target_compile_options(cmake-help-cpp PRIVATE ${PROCESS_LIB_WARNINGS})

  set_target_properties(cmake-help-cpp PROPERTIES
    CXX_CLANG_TIDY "${PROCESS_LIB_CLANG_TIDY}"
  )

  set_target_properties(cmake-help-c cmake-help-cpp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY examples
  )
endif()

### Install ###

# Adapted from https://codingnest.com/basic-cmake-part-2/

include(GNUInstallDirs)
set(PROCESS_LIB_INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/process-lib)

install(
  TARGETS process-lib
  EXPORT process-lib-targets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

target_include_directories(process-lib PUBLIC
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(
  EXPORT process-lib-targets
  FILE process-lib-targets.cmake
  NAMESPACE process-lib::
  DESTINATION ${PROCESS_LIB_INSTALL_CONFIGDIR}
)

install(
  DIRECTORY include/c/process-lib
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(PROCESS_LIB_BUILD_CPP_WRAPPER)
  install(
    DIRECTORY include/cpp/process-lib
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/process-lib-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/process-lib-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/process-lib-config.cmake
    INSTALL_DESTINATION ${PROCESS_LIB_INSTALL_CONFIGDIR}}
)

install(
    FILES
      ${CMAKE_CURRENT_BINARY_DIR}/process-lib-config.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/process-lib-config-version.cmake
    DESTINATION ${PROCESS_LIB_INSTALL_CONFIGDIR}
)
