enable_language(CXX)

add_subdirectory(res)

add_executable(tests "")

add_dependencies(tests echo infinite)

target_include_directories(tests PRIVATE 
  ${CMAKE_SOURCE_DIR}/third-party/doctest
)

target_link_libraries(tests PRIVATE common-warnings process)

target_compile_definitions(tests PRIVATE
  DOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING
  ECHO_PATH="$<TARGET_FILE:echo>"
  INFINITE_PATH="$<TARGET_FILE:infinite>"
)

target_sources(tests PRIVATE
  impl.cpp
  read-write.cpp
  stop.cpp
)
