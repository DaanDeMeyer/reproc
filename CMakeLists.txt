cmake_minimum_required(VERSION 3.1)
project(process-lib LANGUAGES C)

add_library(process "")

# Enable C++11 (Required)
set_property(TARGET process PROPERTY C_STANDARD 99)
set_property(TARGET process PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET process PROPERTY C_EXTENSIONS OFF)

if(MSVC)
  target_compile_options(process PRIVATE /W3)
  target_compile_definitions(process PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(process PRIVATE -Wall)
endif()

target_include_directories(process PUBLIC include)

if(WIN32)
  target_include_directories(process PRIVATE src/windows)
  target_sources(process PRIVATE src/windows/process.c src/windows/util.c)
elseif(UNIX)
  target_include_directories(process PRIVATE src/posix)
  target_sources(process PRIVATE src/posix/process.c)
endif()

add_executable(test test/main.c)
