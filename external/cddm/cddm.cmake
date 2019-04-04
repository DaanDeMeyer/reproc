# CDDM (CMake Daan De Meyer)
# Version: v0.0.2
#
# Description: Encapsulates common CMake configuration for cross-platform
# C/C++ libraries.
#
# Features:
# - Warnings
#   - UNIX: -Wall, -Wextra, ...
#   - Windows: /W4
# - Sanitizers (optional, UNIX only)
# - clang-tidy (optional)
# - Automatic installation including pkg-config and CMake config files.
# - Automatic export header generation.
#
# Options:
#
# Every option is prefixed with the upper cased project name. For example, if
# the project is named reproc every option is prefixed with `REPROC_`.
#
# Installation options:
# - `INSTALL`: Generate installation rules (default: `ON` unless
#   `BUILD_SHARED_LIBS` is false and the project is built via
#   `add_subdirectory`).
# - `INSTALL_CMAKECONFIGDIR`: CMake config files installation directory
#   (default: `${CMAKE_INSTALL_LIBDIR}/cmake`).
# - `INSTALL_PKGCONFIG`: Install pkg-config files (default: `ON`)
# - `INSTALL_PKGCONFIGDIR`: pkg-config files installation directory
#   (default: `${CMAKE_INSTALL_LIBDIR}/pkgconfig`).
#
# Developer options:
# - `SANITIZERS`: Build with sanitizers (default: `OFF`).
# - `TIDY`: Run clang-tidy when building (default: `OFF`).
# - `CI`: Add -Werror or equivalent to the compile flags and clang-tidy
#   (default: `OFF`).
#
# Functions:
# - cddm_add_common
# - cddm_add_library
#
# See https://github.com/DaanDeMeyer/reproc for an example on how to use cddm.

set(PNL ${PROJECT_NAME}) # PROJECT_NAME_LOWER (PNL)
string(TOUPPER ${PROJECT_NAME} PNU) # PROJECT_NAME_UPPER (PNU)

get_directory_property(${PNU}_IS_SUBDIRECTORY PARENT_DIRECTORY)

### Installation options ###

# Don't add libraries to the install target by default if the project is built
# from within another project as a static library.
if(${PNU}_IS_SUBDIRECTORY AND NOT BUILD_SHARED_LIBS)
  option(${PNU}_INSTALL "Generate installation rules." OFF)
else()
  option(${PNU}_INSTALL "Generate installation rules." ON)
endif()

option(${PNU}_INSTALL_PKGCONFIG "Install pkg-config files." ON)

include(GNUInstallDirs)

set(${PNU}_INSTALL_CMAKECONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake
    CACHE STRING "CMake config files installation directory.")
set(${PNU}_INSTALL_PKGCONFIGDIR ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    CACHE STRING "pkg-config files installation directory.")

mark_as_advanced(
  ${PNU}_INSTALL
  ${PNU}_INSTALL_PKGCONFIG
  ${PNU}_INSTALL_CMAKECONFIGDIR
  ${PNU}_INSTALL_PKGCONFIGDIR
)

### Developer options ###

option(${PNU}_TIDY "Run clang-tidy when building.")
option(${PNU}_SANITIZERS "Build with sanitizers.")
option(${PNU}_CI "Add -Werror or equivalent to the compile flags and \
clang-tidy.")

mark_as_advanced(
  ${PNU}_TIDY
  ${PNU}_SANITIZERS
  ${PNU}_CI
)

### clang-tidy ###

if(${PNU}_TIDY)
  # CMake added clang-tidy support in CMake 3.6.
  cmake_minimum_required(VERSION 3.6)
  find_program(${PNU}_CLANG_TIDY_PROGRAM clang-tidy)
  mark_as_advanced(${PNU}_CLANG_TIDY_PROGRAM)

  if(${PNU}_CLANG_TIDY_PROGRAM)
    # Treat clang-tidy warnings as errors when on CI.
    if(${PNU}_CI)
      set(${PNU}_CLANG_TIDY_PROGRAM
          ${${PNU}_CLANG_TIDY_PROGRAM} -warnings-as-errors=*)
    endif()
  else()
    message(FATAL_ERROR "clang-tidy not found")
  endif()
endif()

### Functions ###

# Applies common configuration to `TARGET`. `LANGUAGE` (C or CXX) is used to
# indicate the language of the target. `STANDARD` indicates the standard of the
# language to use and `OUTPUT_DIRECTORY` defines where to put the resulting
# files.
function(cddm_add_common TARGET LANGUAGE STANDARD OUTPUT_DIRECTORY)
  set_target_properties(${TARGET} PROPERTIES
    ${LANGUAGE}_STANDARD ${STANDARD}
    ${LANGUAGE}_STANDARD_REQUIRED ON
    ${LANGUAGE}_EXTENSIONS OFF

    # Only one of these is actually used per target but instead of passing the
    # type of target to the function and setting only the appropriate property
    # we just set all of them to avoid lots of if checks and an extra function
    # parameter.
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY}
    ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY}
  )

  if(${PNU}_TIDY AND ${PNU}_CLANG_TIDY_PROGRAM)
    set_target_properties(${TARGET} PROPERTIES
      # CLANG_TIDY_PROGRAM is a list so we surround it with quotes to pass it as
      # a single argument.
      ${LANGUAGE}_CLANG_TIDY "${${PNU}_CLANG_TIDY_PROGRAM}"
    )
  endif()

  ### Common development flags (warnings + sanitizers + colors) ###

  if(MSVC)
    if(DEFINED CMAKE_${LANGUAGE}_FLAGS)
      # CMake adds /W3 to CMAKE_C_FLAGS and CMAKE_CXX_FLAGS by default which
      # results in cl.exe warnings if we add /W4 as well. To avoid these
      # warnings we replace /W3 with /W4 instead.
      string(REGEX REPLACE
        "[-/]W[1-4]" ""
        CMAKE_${LANGUAGE}_FLAGS
        "${CMAKE_${LANGUAGE}_FLAGS}"
      )
      set(CMAKE_${LANGUAGE}_FLAGS "${CMAKE_${LANGUAGE}_FLAGS} /W4"
          PARENT_SCOPE)
    endif()

    if (LANGUAGE STREQUAL "C")
      include(CheckCCompilerFlag)
      check_c_compiler_flag(/permissive- HAVE_PERMISSIVE)
    else()
      include(CheckCXXCompilerFlag)
      check_cxx_compiler_flag(/permissive- HAVE_PERMISSIVE)
    endif()

    target_compile_options(${TARGET} PRIVATE
      /nologo # Silence MSVC compiler version output.
      $<$<BOOL:${${PNU}_CI}>:/WX> # -Werror
      $<$<BOOL:${HAVE_PERMISSIVE}>:/permissive->
    )

    if(NOT STANDARD STREQUAL "90")
      # MSVC reports non-constant initializers as a nonstandard extension but
      # they've been standardized in C99 so we disable it if we're targeting at
      # least C99.
      target_compile_options(${TARGET} PRIVATE /wd4204)
    endif()

    target_link_libraries(${TARGET} PRIVATE
      -nologo # Silence MSVC linker version output.
    )
  else()
    target_compile_options(${TARGET} PRIVATE
      -Wall
      -Wextra
      -pedantic-errors
      -Wshadow
      -Wconversion
      -Wsign-conversion
      $<$<BOOL:${${PNU}_CI}>:-Werror>
    )
  endif()

  if(${PNU}_SANITIZERS)
    if(MSVC)
      message(FATAL_ERROR "Building with sanitizers is not supported when \
      using the Visual C++ toolchain.")
    endif()

    if(NOT ${CMAKE_${LANGUAGE}_COMPILER_ID} MATCHES GNU|Clang)
      message(FATAL_ERROR "Building with sanitizers is not supported when \
      using the ${CMAKE_${LANGUAGE}_COMPILER_ID} compiler.")
    endif()

    target_compile_options(${TARGET} PRIVATE
      -fsanitize=address,undefined
    )
    target_link_libraries(${TARGET} PRIVATE
      -fsanitize=address,undefined
      # GCC sanitizers only work when using the gold linker.
      $<$<${LANGUAGE}_COMPILER_ID:GNU>:-fuse-ld=gold>
    )
  endif()

  target_compile_options(${TARGET} PRIVATE
    $<$<${LANGUAGE}_COMPILER_ID:GNU>:-fdiagnostics-color>
    $<$<${LANGUAGE}_COMPILER_ID:Clang>:-fcolor-diagnostics>
  )
endfunction()

# Adds a new library with name `TARGET` and applies common configuration to it.
# `LANGUAGE` and `STANDARD` define the language and corresponding standard used
# by the target.
#
# An export header is generated and made available as follows (assuming the
# target is named reproc):
# - `LANGUAGE == C` => `#include <reproc/export.h>`
# - `LANGUAGE == CXX` => `#include <reproc/export.hpp>`
#
# The export header for reproc includes the `REPROC_EXPORT` macro which can be
# applied to any public API functions.
function(cddm_add_library TARGET LANGUAGE STANDARD)
  add_library(${TARGET} "")
  cddm_add_common(${TARGET} ${LANGUAGE} ${STANDARD} lib)

  # Enable -fvisibility=hidden and -fvisibility-inlines-hidden (if applicable).
  set_target_properties(${TARGET} PROPERTIES
    ${LANGUAGE}_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN true
  )

  # A preprocesor macro cannot contain + so we replace it with x.
  string(REPLACE + x EXPORT_MACRO ${TARGET})
  string(TOUPPER ${EXPORT_MACRO} EXPORT_MACRO_UPPER)

  if(LANGUAGE STREQUAL "C")
    set(HEADER_EXT h)
  else()
    set(HEADER_EXT hpp)
  endif()

  # CMake's GenerateExportHeader only recently learned to support C projects.
  if(CMAKE_VERSION VERSION_LESS "3.12")
    enable_language(CXX)
  endif()

  # Generate export headers. We generate export headers using CMake since
  # different export files are required depending on whether a library is shared
  # or static and we can't determine whether a library is shared or static from
  # the export header without requiring the user to add a #define which we want
  # to avoid.
  include(GenerateExportHeader)
  generate_export_header(${TARGET}
    BASE_NAME ${EXPORT_MACRO_UPPER}
    EXPORT_FILE_NAME
      ${CMAKE_CURRENT_BINARY_DIR}/include/${TARGET}/export.${HEADER_EXT}
  )

  # Make sure we follow the popular naming convention for shared libraries on
  # UNIX systems.
  set_target_properties(${TARGET} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
  )

  # Only use the headers from the repository when building. When installing we
  # want to use the install location of the headers (e.g. /usr/include) as the
  # include directory instead.
  target_include_directories(${TARGET} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  )

  # Adapted from https://codingnest.com/basic-cmake-part-2/.
  # Each library is installed separately (with separate config files).

  if(${PNU}_INSTALL)
    include(GNUInstallDirs)

    ## Headers

    install(
      DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/${TARGET}
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
      DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/${TARGET}
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    target_include_directories(${TARGET} PUBLIC
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    ## Libraries

    install(
      TARGETS ${TARGET}
      EXPORT ${TARGET}-targets
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    ## Config files

    # CMake

    install(
      EXPORT ${TARGET}-targets
      FILE ${TARGET}-targets.cmake
      DESTINATION ${${PNU}_INSTALL_CMAKECONFIGDIR}/${TARGET}
    )

    include(CMakePackageConfigHelpers)

    write_basic_package_version_file(
      ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config-version.cmake
      VERSION ${PROJECT_VERSION}
      COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config.cmake
      INSTALL_DESTINATION
        ${${PNU}_INSTALL_CMAKECONFIGDIR}/${TARGET}
    )

    install(
      FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config-version.cmake
      DESTINATION
        ${${PNU}_INSTALL_CMAKECONFIGDIR}/${TARGET}
    )

    # pkg-config

    if(${PNU}_INSTALL_PKGCONFIG)
      configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.pc
        @ONLY
      )

      install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.pc
        DESTINATION ${${PNU}_INSTALL_PKGCONFIGDIR}
      )
    endif()
  endif()
endfunction()
