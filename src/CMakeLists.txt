add_library(process "")

target_include_directories(process PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_sources(process PRIVATE common.c)

if(WIN32)
  target_include_directories(process PRIVATE src/windows)
  target_sources(process PRIVATE windows/process.c windows/util.c)
elseif(UNIX)
  target_include_directories(process PRIVATE src/posix)
  target_sources(process PRIVATE posix/process.c posix/util.c)
endif()

target_link_libraries(process PRIVATE common-warnings)

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
