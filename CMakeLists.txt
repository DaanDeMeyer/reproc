# Every variable is namespaced with REPROC and everything except for the library
# itself is turned off by default to make reproc easy to use as a CMake
# subproject with add_subdirectory.

cmake_minimum_required(VERSION 3.1)

# Use updated policies on newer versions of CMake
if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
    cmake_policy(VERSION 3.12)
endif()

project(reproc VERSION 1.0.0 LANGUAGES C)

# Tip: use cmake-gui when contributing to reproc to turn on everything without
# having to type out all the options by hand (cmake .. && cmake-gui .).
option(REPROC_BUILD_TESTS "Build tests" OFF)
option(REPROC_BUILD_EXAMPLES "Build examples. Enable REPROC_BUILD_CXX_WRAPPER \
to build C++ examples." OFF)
option(REPROC_BUILD_CXX_WRAPPER "Build C++ wrapper" OFF)
option(REPROC_BUILD_DOCS "Build API documentation with Doxygen" OFF)
option(REPROC_CI "Add -Werror or equivalent to compiler and clang-tidy" OFF)
option(REPROC_INSTALL "Generate install target" OFF)
option(REPROC_CLANG_TIDY "Run clang-tidy as part of the build (requires \
clang-tidy to be in the PATH)" OFF)
option(REPROC_CLANG_FORMAT "Find clang-format and add target to run it on all \
source files" OFF)
option(REPROC_SANITIZERS "Compile with sanitizers" OFF)
# CMake adds /W3 to CMAKE_C_FLAGS and CMAKE_CXX_FLAGS by default on Windows so
# when we add /W4 via target_compile_options cl.exe complains that both /W3 and
# /W4 are passed. We can avoid these warnings by replacing /W3 with /W4 in
# CMAKE_CXX_FLAGS but this is intrusive when reproc is used via add_subdirectory
# since it would add /W4 for every target. To solve this, we add this option so
# /W4 can be enabled by developers and in CI without forcing it on for users.
option(REPROC_ENABLE_W4 "Enable /W4 on Windows (modifies CMAKE_C_FLAGS and \
CMAKE_CXX_FLAGS)" OFF)
option(REPROC_COLORED_OUTPUT "Force colored output from GCC and Clang")

# C++

if(REPROC_BUILD_CXX_WRAPPER OR REPROC_BUILD_TESTS)
  enable_language(CXX)
endif()

### clang-tidy ###

if(REPROC_CLANG_TIDY)
  # CMake added clang-tidy support in CMake 3.6.
  cmake_minimum_required(VERSION 3.6)
  find_program(REPROC_CLANG_TIDY_PROGRAM clang-tidy)

  if(REPROC_CLANG_TIDY_PROGRAM)
    # Treat clang-tidy warnings as errors when on CI.
    if (REPROC_CI)
      set(REPROC_CLANG_TIDY_PROGRAM
          ${REPROC_CLANG_TIDY_PROGRAM} -warnings-as-errors=*)
    endif()
  else()
    message(WARNING "clang-tidy not found")
  endif()
endif()

### Common development flags ###

# Target used to propagate common flags to all targets defined by reproc
add_library(reproc-dev-flags INTERFACE)

if(MSVC)
  # See REPROC_ENABLE_W4 option declaration for why we can't just add /W4 as a
  # compile option.
  if(REPROC_ENABLE_W4)
    string(REGEX REPLACE /W[0-4] "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4" )

    string(REGEX REPLACE /W[0-4] "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4" )
  endif()

  include(CheckCCompilerFlag)
  check_c_compiler_flag(/permissive- REPROC_HAS_PERMISSIVE)

  target_compile_options(reproc-dev-flags INTERFACE
    # Silence MSVC compiler version output.
    /nologo
    $<$<BOOL:${REPROC_CI}>:/WX>
    $<$<BOOL:${REPROC_HAS_PERMISSIVE}>:/permissive->
    # MSVC reports non-constant initializers as a nonstandard extension but
    # they've been standardized in C99 and we're targeting C99 so this warning
    # is not valid for us.
    $<$<C_COMPILER_ID:MSVC>:/wd4204>
  )

  target_link_libraries(reproc-dev-flags INTERFACE
    # Silence MSVC linker version output.
    -nologo
    # Disable incremental linking to silence warnings when rebuilding reproc
    # after executing ninja -t clean.
    -INCREMENTAL:NO
  )
else()
  target_compile_options(reproc-dev-flags INTERFACE
    -Wall
    -Wextra
    -pedantic-errors
    -Wshadow
    -Wconversion
    $<$<BOOL:${REPROC_CI}>:-Werror>
  )

  if(REPROC_SANITIZERS)
    target_compile_options(reproc-dev-flags INTERFACE
      -fsanitize=address,undefined
    )
    target_link_libraries(reproc-dev-flags INTERFACE
      -fsanitize=address,undefined
      # GCC sanitizers only work when using the gold linker.
      $<$<C_COMPILER_ID:GNU>:-fuse-ld=gold>
    )
  endif()
endif()

if(REPROC_COLORED_OUTPUT)
  target_compile_options(reproc-dev-flags INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>
    $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>
  )
endif()

### Helpers ###

set(REPROC_C_STANDARD 99)
set(REPROC_CXX_STANDARD 11)

function(reproc_set_common TARGET LANGUAGE OUTPUT_DIRECTORY)
  string(REPLACE "reproc-" "" WITHOUT_PREFIX ${TARGET})

  set_target_properties(${TARGET} PROPERTIES
    ${LANGUAGE}_STANDARD ${REPROC_${LANGUAGE}_STANDARD}
    ${LANGUAGE}_STANDARD_REQUIRED ON
    ${LANGUAGE}_EXTENSIONS OFF
    OUTPUT_NAME ${WITHOUT_PREFIX}
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY}
    ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY}
  )

  target_link_libraries(${TARGET} PRIVATE
    # Avoid error when generating install target (we only need the flags when
    # building anyway).
    $<BUILD_INTERFACE:reproc-dev-flags>
  )

  if(REPROC_CLANG_TIDY AND REPROC_CLANG_TIDY_PROGRAM)
    set_target_properties(${TARGET} PROPERTIES
      # REPROC_CLANG_TIDY is a list so we surround it with quotes to pass it as
      # a single argument.
      ${LANGUAGE}_CLANG_TIDY "${REPROC_CLANG_TIDY_PROGRAM}"
    )
  endif()
endfunction()

### Library ###

add_library(reproc "")
add_library(reproc::reproc ALIAS reproc)

reproc_set_common(reproc C lib)

# Put library files in tests and examples directories on CI so the executables
# in those directories finds reproc.dll without us having to modify the PATH.
if(REPROC_CI AND BUILD_SHARED_LIBS AND WIN32)
  add_custom_command(
    TARGET reproc
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:reproc> tests
  )
  add_custom_command(
    TARGET reproc
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:reproc> examples
  )
endif()

if(NOT MSVC)
  target_compile_options(reproc PRIVATE
    # More or less dllexport/dllimport for GCC and Clang.
    -fvisibility=hidden
  )
endif()

set_target_properties(reproc PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

# # Silence MSVC Library Manager version output.
# if(MSVC)
#   set_property(TARGET reproc APPEND PROPERTY STATIC_LIBRARY_FLAGS /nologo)
# endif()

# Used to add visibility modifiers (see include/c/export.h and
# include/cpp/export.hpp).
target_compile_definitions(reproc
  PUBLIC $<$<BOOL:${BUILD_SHARED_LIBS}>:REPROC_SHARED>
  PRIVATE $<BUILD_INTERFACE:REPROC_BUILDING>
)

# Only use the headers from the repository when building. When installing we
# want to use the install location of the headers (e.g. /usr/include) as the
# include directory which is specified later in this file (Install section).
target_include_directories(reproc PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/c>
)
target_include_directories(reproc PRIVATE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/c/common>
)
target_sources(reproc PRIVATE src/c/common/reproc/error.c)

if(WIN32)
  target_include_directories(reproc PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/c/windows>
  )
  target_sources(reproc PRIVATE
    src/c/windows/reproc/error.c
    src/c/windows/reproc/handle.c
    src/c/windows/reproc/pipe.c
    src/c/windows/reproc/process.c
    src/c/windows/reproc/reproc.c
    src/c/windows/reproc/string_utils.c
  )
elseif(UNIX)
  target_include_directories(reproc PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/c/posix>
  )
  target_sources(reproc PRIVATE
    src/c/posix/reproc/error.c
    src/c/posix/reproc/fork.c
    src/c/posix/reproc/pipe.c
    src/c/posix/reproc/reproc.c
    src/c/posix/reproc/process.c
  )
endif()

if(MSVC)
  # Check if STARTUPINFOEX and related functions are available.
  include(CheckSymbolExists)
  check_symbol_exists(InitializeProcThreadAttributeList windows.h
                      REPROC_ATTRIBUTE_LIST_FOUND)

  target_compile_definitions(reproc PRIVATE
    _CRT_SECURE_NO_WARNINGS
    # Remove unneeded stuff from windows.h.
    WIN32_LEAN_AND_MEAN
    VC_EXTRALEAN
    NOMINMAX
    $<$<BOOL:${REPROC_ATTRIBUTE_LIST_FOUND}>:HAS_ATTRIBUTE_LIST>
  )
elseif(UNIX)
  # Check if pipe2 is available.
  include(CheckSymbolExists)
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  check_symbol_exists(pipe2 unistd.h REPROC_PIPE2_FOUND)
  list(REMOVE_AT CMAKE_REQUIRED_DEFINITIONS -1)

  target_compile_definitions(reproc PRIVATE
    _GNU_SOURCE
    $<$<BOOL:${REPROC_PIPE2_FOUND}>:HAS_PIPE2>
  )
endif()

### C++ wrapper ###

if(REPROC_BUILD_CXX_WRAPPER)
  reproc_set_common(reproc CXX lib)

  target_include_directories(reproc PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/cpp>
  )
  target_sources(reproc PRIVATE
    src/cpp/reproc/reproc.cpp
    src/cpp/reproc/error.cpp
    src/cpp/reproc/parser.cpp
  )
endif()

### Tests ###

if(REPROC_BUILD_TESTS)

  add_executable(reproc-tests "")
  reproc_set_common(reproc-tests CXX tests)

  target_link_libraries(reproc-tests PRIVATE reproc)
  target_include_directories(reproc-tests SYSTEM PRIVATE external/doctest)

  target_sources(reproc-tests PRIVATE
    tests/impl.cpp
    tests/read-write.cpp
    tests/stop.cpp
    tests/working-directory.cpp
  )

  ### Helper programs ###

  function(reproc_add_test_helper TARGET)
    add_executable(reproc-${TARGET} tests/resources/${TARGET}.cpp)
    reproc_set_common(reproc-${TARGET} CXX tests/resources)

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
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )

  add_dependencies(reproc-run-tests reproc-tests)
endif()

### Examples ###

if(REPROC_BUILD_EXAMPLES)

  set(REPROC_C_EXT c)
  set(REPROC_CXX_EXT cpp)

  function(reproc_add_example TARGET LANGUAGE)
    add_executable(reproc-${TARGET} examples/${TARGET}.${REPROC_${LANGUAGE}_EXT})
    reproc_set_common(reproc-${TARGET} ${LANGUAGE} examples)

    target_link_libraries(reproc-${TARGET} PRIVATE reproc)
    if(REPROC_SANITIZERS)
      set_target_properties(reproc-${TARGET} PROPERTIES
        # Hack to avoid UBSAN undefined reference errors.
        LINKER_LANGUAGE CXX
      )
    endif()
  endfunction()

  reproc_add_example(git-status C)

  if(REPROC_BUILD_CXX_WRAPPER)
    reproc_add_example(cmake-help CXX)
    reproc_add_example(forward CXX)
  endif()
endif()

### Install ###

# Adapted from https://codingnest.com/basic-cmake-part-2/.

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
      ${PROJECT_BINARY_DIR}/reproc-config-version.cmake
      VERSION ${PROJECT_VERSION}
      COMPATIBILITY AnyNewerVersion
  )

  configure_package_config_file(
      ${PROJECT_SOURCE_DIR}/tools/cmake/reproc-config.cmake.in
      ${PROJECT_BINARY_DIR}/reproc-config.cmake
      INSTALL_DESTINATION ${REPROC_INSTALL_CONFIGDIR}}
  )

  install(
      FILES
        ${PROJECT_BINARY_DIR}/reproc-config.cmake
        ${PROJECT_BINARY_DIR}/reproc-config-version.cmake
      DESTINATION ${REPROC_INSTALL_CONFIGDIR}
  )

endif()

### clang-format ###

if(REPROC_CLANG_FORMAT)
  find_program(REPROC_CLANG_FORMAT_PROGRAM clang-format)

  if(REPROC_CLANG_FORMAT_PROGRAM)
    # CMake discourages globbing for specifying source files but since we
    # already manually specify our sources there's no harm in using globbing to
    # gather all source files so we can pass them to clang-format.
    file(GLOB_RECURSE REPROC_SOURCES
      include/*
      src/*
      tests/*
      examples/*
    )

    message("${REPROC_SOURCES}")

    add_custom_target(
      reproc-format
      COMMAND ${REPROC_CLANG_FORMAT_PROGRAM} -i ${REPROC_SOURCES}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
  else()
    message(WARNING "clang-format not found")
  endif()
endif()

### Doxygen ###

if(REPROC_BUILD_DOCS)
  find_package(Doxygen 1.8.3 REQUIRED)

  if(DOXYGEN_FOUND)
    configure_file(
      ${PROJECT_SOURCE_DIR}/tools/doxygen/doxyfile.in
      ${PROJECT_BINARY_DIR}/doxyfile
      @ONLY
    )

    if(NOT EXISTS ${PROJECT_BINARY_DIR}/cppreference-doxygen-web.tag.xml)
      # Download tag file to automatically generate links to cppreference from
      # doxygen.
      file(
        DOWNLOAD
        http://upload.cppreference.com/mwiki/images/f/f8/cppreference-doxygen-web.tag.xml
        ${PROJECT_BINARY_DIR}/cppreference-doxygen-web.tag.xml
      )
    endif()

    add_custom_target(
      reproc-docs
      COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/doxyfile
    )
  else()
    message(WARNING "doxygen not found")
  endif()
endif()
