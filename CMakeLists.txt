cmake_minimum_required(VERSION 3.1)
project(process-lib LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)
include(common-warnings)

option(PROCESS_LIB_BUILD_TESTS ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

### process-lib

add_subdirectory(src)

### Tests

if(PROCESS_LIB_BUILD_TESTS)
  add_subdirectory(test)
endif()


