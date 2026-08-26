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

link_libraries(screen)

# OpenGL samples need the xbox360 (Mesa softpipe) GL backend: headers
# GL/gl.h + xbox360/xbox360_api.h and libxbox360.a installed into the
# sysroot.  They are added only when that backend is available so the rest
# of the samples build without it.
find_library(XECHAIN_XBOX360_GL_LIBRARY
    NAMES xbox360
    HINTS "${XECHAIN_SYSROOT}/lib" "${XECHAIN_SYSROOT}/ppc-xbox360/lib")
find_path(XECHAIN_XBOX360_GL_INCLUDE
    NAMES xbox360/xbox360_api.h
    HINTS "${XECHAIN_SYSROOT}/include" "${XECHAIN_SYSROOT}/ppc-xbox360/include")
if(XECHAIN_XBOX360_GL_LIBRARY AND XECHAIN_XBOX360_GL_INCLUDE)
    add_subdirectory(gl)
    add_subdirectory(gpu)
    message(STATUS "OpenXeChain GL samples enabled (xbox360 backend found)")
else()
    message(STATUS "OpenXeChain GL samples skipped (xbox360 backend not installed; "
                   "build the xbox360 Mesa target and cmake --install it into "
                   "${XECHAIN_SYSROOT})")
endif()

message(STATUS "OpenXeChain samples configured (sysroot: ${XECHAIN_SYSROOT})")
