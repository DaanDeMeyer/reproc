# Every variable is prefixed with the name of the project and everything option
# is turned off by default to make projects using cddm easy to use as a CMake
# subproject with add_subdirectory. All targets are also prefixed with the name
# of the project to avoid target name collisions.

set(PNL ${PROJECT_NAME}) # PROJECT_NAME_LOWER (PNL)
string(TOUPPER ${PROJECT_NAME} PNU) # PROJECT_NAME_UPPER (PNU)

get_directory_property(${PNU}_IS_SUBDIRECTORY PARENT_DIRECTORY)

### User options ###

option(${PNU}_TESTS "Build tests.")
option(${PNU}_EXAMPLES "Build examples.")

# Don't add libraries to the install target by default if the project is built
# from within another project as a static library.
if(${PNU}_IS_SUBDIRECTORY AND NOT BUILD_SHARED_LIBS)
  option(${PNU}_INSTALL "Install targets." OFF)
else()
  option(${PNU}_INSTALL "Install targets." ON)
endif()

### Developer options

option(${PNU}_TIDY "Run clang-tidy when building.")
option(${PNU}_SANITIZERS "Build with sanitizers.")
option(${PNU}_CI "Add -Werror or equivalent to the compiler and clang-tidy.")

if(${PNU}_TIDY)
  # CMake added clang-tidy support in CMake 3.6.
  cmake_minimum_required(VERSION 3.6)
  find_program(${PNU}_CLANG_TIDY_PROGRAM clang-tidy)

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

# Encapsulates common configuration for a target.
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
    # CMake adds /W3 to CMAKE_C_FLAGS and CMAKE_CXX_FLAGS by default on Windows
    # so when we add /W4 via target_compile_options cl.exe complains that both
    # /W3 and /W4 are passed. We can avoid these warnings by replacing /W3 with
    # /W4 in CMAKE_CXX_FLAGS but this is intrusive when a project is used via
    # add_subdirectory since it would add /W4 to every target. To solve this, we
    # only add /W4 when the project is not included with add_subdirectory.
    if(NOT ${PNU}_IS_SUBDIRECTORY)
      string(REGEX REPLACE
        /W[0-3] ""
        CMAKE_${LANGUAGE}_FLAGS
        "${CMAKE_${LANGUAGE}_FLAGS}"
      )

      string(FIND ${CMAKE_${LANGUAGE}_FLAGS} "/W4" W4_FOUND)
      if(NOT W4_FOUND)
        set(CMAKE_${LANGUAGE}_FLAGS "${CMAKE_${LANGUAGE}_FLAGS} /W4"
            CACHE STRING "" FORCE)
      endif()
    endif()

    if (${LANGUAGE} STREQUAL C)
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

    if(NOT ${STANDARD} STREQUAL 90)
      # MSVC reports non-constant initializers as a nonstandard extension but
      # they've been standardized in C99 so we disable it if we're targeting at
      # least C99.
      target_compile_options(${TARGET} PRIVATE /wd4204)
    endif()

    target_link_libraries(${TARGET} PRIVATE
      -nologo # Silence MSVC linker version output.
      # Disable incremental linking to silence warnings when rebuilding after
      # executing ninja -t clean.
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

# Adds a new library and encapsulates common configuration for it.
function(cddm_add_library TARGET LANGUAGE STANDARD)
  add_library(${TARGET} "")
  add_library(${PNL}::${TARGET} ALIAS ${TARGET})
  cddm_add_common(${TARGET} ${LANGUAGE} ${STANDARD} lib)

  # Enable -fvisibility=hidden and -fvisibility-inlines-hidden (if applicable).
  set_target_properties(${TARGET} PROPERTIES
    ${LANGUAGE}_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN true
  )

  # A preprocesor macro cannot contain + so we replace it with x.
  string(REPLACE + x EXPORT_MACRO ${TARGET})
  string(TOUPPER ${EXPORT_MACRO} EXPORT_MACRO_UPPER)

  if(${LANGUAGE} STREQUAL C)
    set(HEADER_EXT h)
  else()
    set(HEADER_EXT hpp)
  endif()

  # CMake's GenerateExportHeader only recently learned to support C projects.
  if(${CMAKE_VERSION} VERSION_LESS 3.12)
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

    # Headers

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

    # Libraries

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
      NAMESPACE ${PROJECT_NAME}::
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET}
    )

    # CMake config

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
        ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET}
    )

    install(
      FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config-version.cmake
      DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET}
    )

    # pkg-config

    set(INSTALL_PKGCONFIGDIR )

    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}.pc.in
      ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.pc
      @ONLY
    )

    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.pc
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
  endif()
endfunction()
