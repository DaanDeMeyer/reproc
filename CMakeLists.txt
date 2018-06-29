cmake_minimum_required(VERSION 3.1)
project(process-lib LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)
include(common-warnings)

option(PROCESS_LIB_BUILD_TESTS OFF)
option(PROCESS_LIB_BUILD_EXAMPLES OFF)
option(PROCESS_LIB_BUILD_CPP_WRAPPER OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

### src ###

add_library(process "")

target_link_libraries(process PRIVATE common-warnings)
set_target_properties(process PROPERTIES ARCHIVE_OUTPUT_DIRECTORY lib)

target_include_directories(process PUBLIC include/c)

if(WIN32)
  target_include_directories(process PRIVATE src/c/windows)
  target_sources(process PRIVATE src/c/windows/process.c src/c/windows/util.c)
elseif(UNIX)
  target_include_directories(process PRIVATE src/c/posix)
  target_sources(process PRIVATE src/c/posix/process.c src/c/posix/util.c)
endif()

if(MSVC)
  target_compile_definitions(process PRIVATE 
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    VC_EXTRALEAN
    NOMINMAX
  )
else()
  target_compile_options(process PRIVATE
    -Wstrict-prototypes
    -Wmissing-prototypes
  )
  target_compile_definitions(process PRIVATE
    _POSIX_C_SOURCE
  )
endif()

### C++ Wrapper ###

if(PROCESS_LIB_BUILD_CPP_WRAPPER)
  enable_language(CXX)

  target_include_directories(process PUBLIC include/cpp)
  target_sources(process PRIVATE src/cpp/process.cpp)
endif()

### Tests ###

if(PROCESS_LIB_BUILD_TESTS)
  enable_language(CXX)

  add_executable(echo test/res/echo.cpp)
  target_link_libraries(echo PRIVATE common-warnings)

  add_executable(infinite test/res/infinite.cpp)
  target_link_libraries(infinite PRIVATE common-warnings)

  set_target_properties(echo infinite PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY test/res
  )

  add_executable(tests "")
  
  add_dependencies(tests echo infinite)
  set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY test)
  
  target_link_libraries(tests PRIVATE common-warnings process)
  target_include_directories(tests PRIVATE third-party/doctest/doctest)

  target_compile_definitions(tests PRIVATE
    DOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING
    ECHO_PATH="$<TARGET_FILE:echo>"
    INFINITE_PATH="$<TARGET_FILE:infinite>"
  )

  target_sources(tests PRIVATE
    test/impl.cpp
    test/read-write.cpp
    test/stop.cpp
  )
endif()

### Examples ###

if(PROCESS_LIB_BUILD_EXAMPLES)  
  add_executable(cmake-help examples/cmake-help.c)
  target_link_libraries(cmake-help PRIVATE common-warnings process)
  set_target_properties(cmake-help PROPERTIES RUNTIME_OUTPUT_DIRECTORY examples)
endif()

