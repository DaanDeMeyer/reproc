if(WIN32)
  set(REPROC_WINSOCK_LIBRARY ws2_32)
endif()

reproc_add_library(reproc C)

if(REPROC_MULTITHREADED)
  target_compile_definitions(reproc PRIVATE REPROC_MULTITHREADED)
  target_link_libraries(reproc PRIVATE Threads::Threads)
endif()

if(WIN32)
  set(PLATFORM win32)
  target_compile_definitions(reproc PRIVATE WIN32_LEAN_AND_MEAN)
  target_link_libraries(reproc PRIVATE ${REPROC_WINSOCK_LIBRARY})
else()
  set(PLATFORM posix)
endif()

target_sources(reproc PRIVATE
  src/clock.${PLATFORM}.c
  src/drain.c
  src/error.${PLATFORM}.c
  src/handle.${PLATFORM}.c
  src/init.${PLATFORM}.c
  src/options.c
  src/pipe.${PLATFORM}.c
  src/process.${PLATFORM}.c
  src/redirect.c
  src/redirect.${PLATFORM}.c
  src/reproc.c
  src/run.c
)

reproc_add_test(reproc argv C)
reproc_add_test(reproc environment C)
reproc_add_test(reproc io C)
reproc_add_test(reproc overflow C)
reproc_add_test(reproc stop C)
reproc_add_test(reproc working-directory C)

if(UNIX)
  reproc_add_test(fork)
endif()

reproc_add_example(reproc drain C)
reproc_add_example(reproc file C)
reproc_add_example(reproc read C)
reproc_add_example(reproc parent C)
reproc_add_example(reproc run C)
