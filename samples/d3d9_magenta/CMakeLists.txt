# d3d9_magenta: magenta screen clear using Direct3D 9 types.

add_executable(d3d9_magenta main.cpp)
target_link_libraries(d3d9_magenta PRIVATE screen)
openxechain_add_xex(d3d9_magenta)