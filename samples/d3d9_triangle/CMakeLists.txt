# d3d9_triangle: a triangle drawn using Direct3D 9 vertex/colour types.

add_executable(d3d9_triangle main.cpp)
target_link_libraries(d3d9_triangle PRIVATE screen)
openxechain_add_xex(d3d9_triangle)