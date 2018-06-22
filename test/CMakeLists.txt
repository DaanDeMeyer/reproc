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
)

target_sources(tests PRIVATE
  doctest_impl.cpp
  read-write.cpp
  terminate.cpp
)

add_custom_target(test
  COMMAND tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}
)