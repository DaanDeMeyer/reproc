cmake_minimum_required(VERSION 3.1)
project(cpp-process)

add_library(cpp-process "")

# Enable C++11 (Required)
set_property(TARGET cpp-process PROPERTY CXX_STANDARD 14)
set_property(TARGET cpp-process PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET cpp-process PROPERTY CXX_EXTENSIONS OFF)

if(MSVC)
  target_compile_options(cpp-process PRIVATE /W3)
else()
  target_compile_options(cpp-process PRIVATE -Wall)
endif()

target_sources(cpp-process PRIVATE src/process.cpp)

if(WIN32)
  target_sources(cpp-process PRIVATE src/process_windows.cpp)
elseif(UNIX)
  target_sources(cpp-process PRIVATE src/process_posix.cpp)
endif()
