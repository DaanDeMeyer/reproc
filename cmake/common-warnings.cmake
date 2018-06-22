
add_library(common-warnings INTERFACE)

if(MSVC)
  target_compile_options(common-warnings INTERFACE 
    $<BUILD_INTERFACE:
      /W3
      /WX
    >
  )
else()
  target_compile_options(common-warnings INTERFACE 
    $<BUILD_INTERFACE:
      -Werror
      -Wall 
      -Wextra 
      -pedantic-errors
      -Wshadow
    >
  )
endif()