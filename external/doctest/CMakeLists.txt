add_library(doctest OBJECT)

target_sources(doctest PRIVATE main.cpp)
target_compile_features(doctest PUBLIC cxx_std_11)
target_include_directories(doctest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# We change the working directory to the build directory before executing the
# tests so tests can reliably find any required resources.
target_compile_definitions(doctest PRIVATE
  DOCTEST_WORKING_DIRECTORY="${PROJECT_BINARY_DIR}"
)