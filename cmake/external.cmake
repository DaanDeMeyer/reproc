if(REPROC_TEST)
  if(NOT EXISTS ${PROJECT_BINARY_DIR}/doctest)
    file(
      DOWNLOAD
      https://raw.githubusercontent.com/onqtam/doctest/2.3.5/doctest/doctest.h
      ${PROJECT_BINARY_DIR}/doctest/doctest.h
    )
  endif()

  add_library(doctest INTERFACE)
  target_compile_features(doctest INTERFACE cxx_std_11)
  target_include_directories(doctest INTERFACE ${PROJECT_BINARY_DIR}/doctest)
endif()
