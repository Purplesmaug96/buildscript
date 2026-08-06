# d3d9_cube: a rotating 3D cube using Direct3D 9 types.

add_executable(d3d9_cube main.cpp)
target_link_libraries(d3d9_cube PRIVATE screen "${XECHAIN_SYSROOT}/ppc-xbox360/lib/libm.a")
openxechain_add_xex(d3d9_cube)