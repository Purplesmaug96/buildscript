# OpenXeChain sample programs.
#
# This directory is part of the OpenXeChain buildscript repository. It is
# meant to be configured with the bundled cross-compile toolchain file:
#
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/ppc-xbox360-toolchain.cmake
#   cmake --build build
#
# Every sample links to a PE32 "XBOX" image and is then converted to a
# runnable .xex by SynthXEX as a post-build step.

cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/XEX.cmake")

add_subdirectory(common)
add_subdirectory(hello)
add_subdirectory(magenta)
add_subdirectory(fileio)
add_subdirectory(threads)
add_subdirectory(cpp)

message(STATUS "OpenXeChain samples configured (sysroot: ${XECHAIN_SYSROOT})")
