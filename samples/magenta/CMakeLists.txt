add_executable(magenta main.c)
target_link_libraries(magenta PRIVATE screen)
openxechain_add_xex(magenta)
