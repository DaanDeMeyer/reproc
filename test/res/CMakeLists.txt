
add_executable(echo echo.cpp)
target_link_libraries(echo common-warnings)

add_executable(infinite infinite.cpp)
target_link_libraries(infinite common-warnings)