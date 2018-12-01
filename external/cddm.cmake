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

option(${PNU}_CI "Add -Werror or equivalent to the compiler and clang-tidy.")
option(${PNU}_TIDY "Run clang-tidy during the build.")
option(${PNU}_SANITIZERS "Build with sanitizers. Only works on UNIX systems.")

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

include(CheckCCompilerFlag)

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
      # CMAKE_C_FLAGS and CMAKE_CXX_FLAGS only get their default values if they
      # haven't been modified yet when the respective language is enabled in
      # CMake. We check to see if they have been set yet so we don't prevent
      # them from taking their default values if their corresponding language is
      # enabled after calling cddm_add_library.
      if(CMAKE_C_FLAGS)
        string(REGEX REPLACE /W[0-4] "/W4" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "" FORCE)
      endif()

      if(CMAKE_CXX_FLAGS)
        string(REGEX REPLACE /W[0-4] "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)
      endif()
    endif()

    check_c_compiler_flag(/permissive- ${PNU}_HAS_PERMISSIVE)

    target_compile_options(${TARGET} PRIVATE
      /nologo # Silence MSVC compiler version output.
      $<$<BOOL:${${PNU}_CI}>:/WX> # -Werror
      $<$<BOOL:${${PNU}_HAS_PERMISSIVE}>:/permissive->
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

# CMake by default sets CMAKE_INSTALL_PREFIX to
# "C:\Program Files (x86)\${PROJECT_NAME}" on Windows which blocks us from using
# find_package to find specific libraries because of find_package's search
# rules. To get around this we remove ${PROJECT_NAME} from CMAKE_INSTALL_PREFIX
# if it hasn't been explicitly set by the user.
if(NOT ${PNU}_SUBDIRECTORY AND WIN32 AND ${PNU}_INSTALL)
  # Needed for CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT.
  cmake_minimum_required(VERSION 3.7)

  # Note: CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT is only true on CMake's
  # first run without a custom CMAKE_INSTALL_PREFIX.
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    get_filename_component(${PNU}_CMAKE_INSTALL_PREFIX_WITHOUT_PROJECT_NAME
                          ${CMAKE_INSTALL_PREFIX} PATH)
    set(CMAKE_INSTALL_PREFIX
        ${${PNU}_CMAKE_INSTALL_PREFIX_WITHOUT_PROJECT_NAME}
        CACHE PATH "" FORCE)
  endif()
endif()

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

  set(GENERATED_HEADERS_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/include)

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
    EXPORT_FILE_NAME ${GENERATED_HEADERS_DIR}/${TARGET}/export.${HEADER_EXT}
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
    $<BUILD_INTERFACE:${GENERATED_HEADERS_DIR}>
  )

  # Adapted from https://codingnest.com/basic-cmake-part-2/.
  # Each library is installed separately (with separate config files).

  if(${PNU}_INSTALL)
    # Add ${TARGET} to install prefix on Windows because CMake searches for
    # <prefix>/<name>/... on Windows instead of <prefix>/....
    if(WIN32)
      set(INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/${TARGET})
    else()
      set(INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
    endif()

    include(GNUInstallDirs)

    target_include_directories(${TARGET} PUBLIC
      $<INSTALL_INTERFACE:${INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}>
    )

    set(INSTALL_CMAKECONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET})

    install(
      TARGETS ${TARGET}
      EXPORT ${TARGET}-targets
      RUNTIME DESTINATION ${INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
    )

    install(
      EXPORT ${TARGET}-targets
      FILE ${TARGET}-targets.cmake
      NAMESPACE ${PROJECT_NAME}::
      DESTINATION ${INSTALL_PREFIX}/${INSTALL_CMAKECONFIGDIR}
    )

    install(
      DIRECTORY include/${TARGET}
      DESTINATION ${INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
      DIRECTORY ${GENERATED_HEADERS_DIR}/${TARGET}
      DESTINATION ${INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}
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
        ${INSTALL_PREFIX}/${INSTALL_CMAKECONFIGDIR}
    )

    install(
      FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config-version.cmake
      DESTINATION ${INSTALL_PREFIX}/${INSTALL_CMAKECONFIGDIR}
    )

    ### pkg-config ###

    set(INSTALL_PKGCONFIGDIR ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

    configure_file(
      ${TARGET}.pc.in
      ${TARGET}.pc
      @ONLY
    )

    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.pc
      DESTINATION ${INSTALL_PREFIX}/${INSTALL_PKGCONFIGDIR}
    )
  endif()
endfunction()
