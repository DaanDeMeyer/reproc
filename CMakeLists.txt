# Every variable is prefixed with REPROC and everything except for the C library
# (reproc) is turned off by default to make reproc easy to use as a CMake
# subproject with add_subdirectory. All targets are also prefixed with "reproc-"
# to avoid name collisions.

cmake_minimum_required(VERSION 3.1)

# Use updated policies on newer versions of CMake.
if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
  cmake_policy(VERSION 3.12)
endif()

project(reproc VERSION 2.0.0 LANGUAGES C)

# Tip: use cmake-gui when contributing to reproc to turn on everything without
# having to type out all the options by hand (cmake .. && cmake-gui .).

option(REPROC_BUILD_TESTS "Build reproc's tests. Requires C++ compiler.")
option(REPROC_BUILD_EXAMPLES "Build the examples. Enable REPROCXX to build the \
C++ examples.")
option(REPROCXX "Build reproc++ (C++ wrapper library).")
option(REPROC_BUILD_DOCS "Build the API documentation with Doxygen.")
option(REPROC_CI "Add -Werror or equivalent to the compiler and clang-tidy.")
option(REPROC_INSTALL "Generate reproc install target. If REPROCXX is enabled \
an install target is also generated for reproc++.")
option(REPROC_CLANG_TIDY "Run clang-tidy as part of the build (requires \
clang-tidy to be in the PATH).")
option(REPROC_CLANG_FORMAT "Find clang-format and add a target to run it on \
all source files.")
option(REPROC_SANITIZERS "Compile reproc with sanitizers. Only works on Unix \
systems.")
# CMake adds /W3 to CMAKE_C_FLAGS and CMAKE_CXX_FLAGS by default on Windows so
# when we add /W4 via target_compile_options cl.exe complains that both /W3 and
# /W4 are passed. We can avoid these warnings by replacing /W3 with /W4 in
# CMAKE_CXX_FLAGS but this is intrusive when reproc is used via add_subdirectory
# since it would add /W4 for every target. To solve this, we add this option so
# /W4 can be enabled by developers and in CI without forcing it on for users.
option(REPROC_ENABLE_W4 "Enable /W4 on Windows (modifies CMAKE_C_FLAGS and \
CMAKE_CXX_FLAGS).")
option(REPROC_COLORED_OUTPUT "Force colored output from GCC and Clang.")

# C++

if(REPROCXX OR REPROC_BUILD_TESTS)
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

### Helpers ###

if(MSVC)
  # See REPROC_ENABLE_W4 option declaration for why we can't just add /W4 as a
  # compile option.
  if(REPROC_ENABLE_W4)
    string(REGEX REPLACE /W[0-4] "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4" )

    string(REGEX REPLACE /W[0-4] "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4" )
  endif()

  # Check for permissive- here to avoid doing the check multiple times in
  # reproc_set_common.
  include(CheckCCompilerFlag)
  check_c_compiler_flag(/permissive- REPROC_HAS_PERMISSIVE)
endif()

# Encapsulates common configuration for a target.
function(reproc_set_common TARGET OUTPUT_DIRECTORY)
  # reproc build output is in its own directory so the output files don't need
  # to be prefixed (only the actual targets in CMake).
  string(REPLACE "reproc-" "" WITHOUT_PREFIX ${TARGET})

  set_target_properties(${TARGET} PROPERTIES
    OUTPUT_NAME ${WITHOUT_PREFIX}

    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF

    # Only one of these is actually used per target but instead of passing the
    # type of target to the function and setting only the appropriate property
    # we just set all of them to avoid lots of if checks and an extra function
    # parameter.
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY}
    ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY}
  )

  if(REPROC_CLANG_TIDY AND REPROC_CLANG_TIDY_PROGRAM)
    set_target_properties(${TARGET} PROPERTIES
      # REPROC_CLANG_TIDY is a list so we surround it with quotes to pass it as
      # a single argument.
      C_CLANG_TIDY "${REPROC_CLANG_TIDY_PROGRAM}"
      CXX_CLANG_TIDY "${REPROC_CLANG_TIDY_PROGRAM}"
    )
  endif()

  ### Common development flags (warnings + sanitizers + colors) ###

  if(MSVC)
    target_compile_options(${TARGET} PRIVATE
      /nologo # Silence MSVC compiler version output.
      $<$<BOOL:${REPROC_CI}>:/WX> # -Werror
      $<$<BOOL:${REPROC_HAS_PERMISSIVE}>:/permissive->
      # MSVC reports non-constant initializers as a nonstandard extension but
      # they've been standardized in C99 and we're targeting C99 so this warning
      # is not valid for us.
      $<$<C_COMPILER_ID:MSVC>:/wd4204>
    )

    target_link_libraries(${TARGET} PRIVATE
      -nologo # Silence MSVC linker version output.
      # Disable incremental linking to silence warnings when rebuilding reproc
      # after executing ninja -t clean.
      -INCREMENTAL:NO
    )
  else()
    target_compile_options(${TARGET} PRIVATE
      -Wall
      -Wextra
      -pedantic-errors
      -Wshadow
      -Wconversion
      -Wsign-conversion
      $<$<BOOL:${REPROC_CI}>:-Werror>
    )

    if(REPROC_SANITIZERS)
      target_compile_options(${TARGET} PRIVATE
        -fsanitize=address,undefined
      )
      target_link_libraries(${TARGET} PRIVATE
        -fsanitize=address,undefined
        # GCC sanitizers only work when using the gold linker.
        $<$<C_COMPILER_ID:GNU>:-fuse-ld=gold>
      )
    endif()
  endif()

  if(REPROC_COLORED_OUTPUT)
    target_compile_options(${TARGET} PRIVATE
      $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>
      $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>
    )
  endif()
endfunction()

# Adds a new library and encapsulates common configuration for it.
function(reproc_add_library TARGET LANGUAGE OUTPUT_DIRECTORY)
  add_library(${TARGET} "")
  add_library(reproc::${TARGET} ALIAS ${TARGET})
  reproc_set_common(${TARGET} ${OUTPUT_DIRECTORY})

  # Put library files in tests and examples directories on CI so the executables
  # in those directories find the dll's without us having to modify the PATH.
  if(REPROC_CI AND BUILD_SHARED_LIBS AND WIN32)
    add_custom_command(
      TARGET ${TARGET}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET}> tests
    )
    add_custom_command(
      TARGET ${TARGET}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET}> examples
    )
  endif()

  if(NOT MSVC)
    target_compile_options(${TARGET} PRIVATE
      # More or less dllexport/dllimport for GCC and Clang.
      -fvisibility=hidden
      # -fvisibility-inlines-hidden is only valid for C++.
      $<$<STREQUAL:${LANGUAGE},CXX>:-fvisibility-inlines-hidden>
    )
  endif()

  # Make sure we follow the popular naming convention for shared libraries on
  # Unix systems.
  set_target_properties(${TARGET} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
  )

  # Used to add visibility modifiers (see include/reproc/export.h and
  # include/reproc++/export.hpp).
  target_compile_definitions(${TARGET}
    PUBLIC $<$<BOOL:${BUILD_SHARED_LIBS}>:REPROC_SHARED>
    PRIVATE $<BUILD_INTERFACE:REPROC_BUILDING>
  )

  # Only use the headers from the repository when building. When installing we
  # want to use the install location of the headers (e.g. /usr/include) as the
  # include directory instead.
  target_include_directories(${TARGET} PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  # Adapted from https://codingnest.com/basic-cmake-part-2/.
  # Each library is installed separately (with separate config files).

  if(REPROC_INSTALL)
    include(GNUInstallDirs)
    set(REPROC_INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET})

    install(
      TARGETS ${TARGET}
      EXPORT ${TARGET}-targets
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(
      EXPORT ${TARGET}-targets
      FILE ${TARGET}-targets.cmake
      NAMESPACE reproc::
      DESTINATION ${REPROC_INSTALL_CONFIGDIR}
    )

    install(
      DIRECTORY include/${TARGET}
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
      ${PROJECT_BINARY_DIR}/${TARGET}-config-version.cmake
      VERSION ${PROJECT_VERSION}
      COMPATIBILITY AnyNewerVersion
    )

    configure_package_config_file(
      ${PROJECT_SOURCE_DIR}/tools/cmake/${TARGET}-config.cmake.in
      ${PROJECT_BINARY_DIR}/${TARGET}-config.cmake
      INSTALL_DESTINATION ${REPROC_INSTALL_CONFIGDIR}
    )

    install(
      FILES
        ${PROJECT_BINARY_DIR}/${TARGET}-config.cmake
        ${PROJECT_BINARY_DIR}/${TARGET}-config-version.cmake
      DESTINATION ${REPROC_INSTALL_CONFIGDIR}
    )
  endif()
endfunction()

### reproc Library ###

reproc_add_library(reproc C lib)

target_include_directories(reproc PRIVATE
  ${PROJECT_SOURCE_DIR}/src/reproc/common>
)
target_sources(reproc PRIVATE src/reproc/common/error.c)

if(WIN32)
  target_include_directories(reproc PRIVATE
    ${PROJECT_SOURCE_DIR}/src/reproc/windows>
  )
  target_sources(reproc PRIVATE
    src/reproc/windows/error.c
    src/reproc/windows/handle.c
    src/reproc/windows/pipe.c
    src/reproc/windows/process.c
    src/reproc/windows/reproc.c
    src/reproc/windows/string_utils.c
  )
elseif(UNIX)
  target_include_directories(reproc PRIVATE
    ${PROJECT_SOURCE_DIR}/src/reproc/posix>
  )
  target_sources(reproc PRIVATE
    src/reproc/posix/error.c
    src/reproc/posix/fork.c
    src/reproc/posix/pipe.c
    src/reproc/posix/reproc.c
    src/reproc/posix/process.c
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
    _GNU_SOURCE # Needed for pipe2 and kill.
    $<$<BOOL:${REPROC_PIPE2_FOUND}>:HAS_PIPE2>
  )
endif()

### reproc++ Library (C++ wrapper) ###

if(REPROCXX)
  reproc_add_library(reproc++ CXX lib)

  target_link_libraries(reproc++ PRIVATE reproc)
  target_sources(reproc++ PRIVATE
    src/reproc++/reproc.cpp
    src/reproc++/error.cpp
    src/reproc++/parser.cpp
  )
endif()

### Tests ###

if(REPROC_BUILD_TESTS)
  add_executable(reproc-tests "")
  reproc_set_common(reproc-tests tests)

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
    reproc_set_common(reproc-${TARGET} tests/resources)

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

  function(reproc_add_example TARGET LANGUAGE)
    add_executable(reproc-${TARGET} "")
    reproc_set_common(reproc-${TARGET} examples)

    if(${LANGUAGE} STREQUAL C)
      target_link_libraries(reproc-${TARGET} PRIVATE reproc)
      target_sources(reproc-${TARGET} PRIVATE examples/${TARGET}.c)
    elseif(${LANGUAGE} STREQUAL CXX)
      target_link_libraries(reproc-${TARGET} PRIVATE reproc++)
      target_sources(reproc-${TARGET} PRIVATE examples/${TARGET}.cpp)
    endif()

    if(REPROC_SANITIZERS)
      set_target_properties(reproc-${TARGET} PROPERTIES
        # Hack to avoid UBSAN undefined reference errors.
        LINKER_LANGUAGE CXX
      )
    endif()
  endfunction()

  reproc_add_example(git-status C)

  if(REPROCXX)
    reproc_add_example(cmake-help CXX)
    reproc_add_example(forward CXX)
  endif()
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
      # Doxygen.
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
