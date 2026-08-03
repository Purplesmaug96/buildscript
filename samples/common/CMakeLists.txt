# Shared code for the OpenXeChain samples: framebuffer access and text.

add_library(screen STATIC screen.c)
target_include_directories(screen PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
