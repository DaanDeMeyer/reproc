cmake_minimum_required(VERSION 3.1)
project(process-lib LANGUAGES C)

add_library(process "")

# Enable C++11 (Required)
set_property(TARGET process PROPERTY C_STANDARD 99)
set_property(TARGET process PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET process PROPERTY C_EXTENSIONS OFF)

if(MSVC)
  target_compile_options(process PRIVATE 
    /W3
  )
  target_compile_definitions(process PRIVATE 
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    VC_EXTRALEAN
    NOMINMAX
  )
else()
  target_compile_options(process PRIVATE 
    -Werror
    -Wall 
    -Wextra 
    -pedantic-errors
    -Wshadow
    -Wcast-qual
    -Wstrict-prototypes
    -Wmissing-prototypes
  )
  target_compile_definitions(process PRIVATE
    _POSIX_C_SOURCE
  )
endif()

target_include_directories(process PUBLIC include)

if(WIN32)
  target_include_directories(process PRIVATE src/windows)
  target_sources(process PRIVATE src/windows/process.c src/windows/util.c)
elseif(UNIX)
  target_include_directories(process PRIVATE src/posix)
  target_sources(process PRIVATE src/posix/process.c src/posix/util.c)
endif()


add_executable(echo test/helpers/echo.c)

add_executable(test "")
target_link_libraries(test process)
target_sources(test PRIVATE
  test/simple-process.c  
)

