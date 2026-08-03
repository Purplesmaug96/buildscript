# OpenXeChain buildscript - sample programs
#
# This project builds the example programs for the OpenXeChain toolchain.
# It MUST be configured with the bundled cross-compile toolchain file, which
# points CMake at the OpenXeChain sysroot (Clang + Newlib + xecorelib):
#
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/ppc-xbox360-toolchain.cmake
#   cmake --build build
#
# See samples/README.md for details on what each sample does and how to run
# the resulting .xex files.

cmake_minimum_required(VERSION 3.20)
project(OpenXeChainSamples)

if(NOT DEFINED XECHAIN_SYSROOT)
    message(FATAL_ERROR
        "This project must be configured with the OpenXeChain toolchain:\n"
        "  cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/ppc-xbox360-toolchain.cmake")
endif()

add_subdirectory(samples)
