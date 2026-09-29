# ============================================================================
# OpenXeChain CMake toolchain file for the ppc32-xbox360 target
#
# A CMake toolchain for developing Xbox 360 homebrew with the OpenXeChain
# toolchain. The sysroot contains Clang/LLVM (with lld and compiler-rt),
# Newlib (libc), and xecorelib (xboxkrnl/XAM/HV import libraries).
#
# Usage:
#   cmake -B build \
#       -DCMAKE_TOOLCHAIN_FILE=/path/to/ppc-xbox360-toolchain.cmake
#
# The sysroot location is resolved in this order:
#   1. -DXECHAIN_SYSROOT=/path/to/sysroot  (CMake cache variable)
#   2. $XECHAIN_ROOT environment variable
#   3. <this file's repository>/sysroot
#
# Note: clang reads its configuration from clang.cfg/clang++.cfg which sit
# next to the compiler binary inside the sysroot. Those files supply the
# target flags (sysroot, Newlib/xecorelib/compiler-rt linkage, -mlongcall,
# -fdeclspec, ...), so the sysroot must not be moved away from its compilers
# unless the .cfg files are updated too.
# ============================================================================

# CMake looks for Platform/${CMAKE_SYSTEM_NAME}.cmake through CMAKE_MODULE_PATH
# (see Modules/CMakeSystemSpecificInformation.cmake), so let it find the
# sibling Platform/xbox360.cmake.  This must happen before the double-load
# guard below, otherwise try_compile sub-projects that take the early return
# would print "System is unknown to cmake" again.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)

if(CMAKE_C_COMPILER)
    # Toolchain file is being loaded twice (e.g. CMake >= 3.21 loads it for
    # try_compile sub-projects); the toolchain is already set up.
    return()
endif()

# --- Locate the OpenXeChain sysroot -----------------------------------------

if(NOT DEFINED XECHAIN_SYSROOT)
    if(DEFINED ENV{XECHAIN_ROOT} AND NOT "$ENV{XECHAIN_ROOT}" STREQUAL "")
        set(XECHAIN_SYSROOT "$ENV{XECHAIN_ROOT}")
    else()
        get_filename_component(_XECHAIN_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
        set(XECHAIN_SYSROOT "${_XECHAIN_REPO_ROOT}/sysroot")
    endif()
endif()
get_filename_component(XECHAIN_SYSROOT "${XECHAIN_SYSROOT}" ABSOLUTE)
set(XECHAIN_SYSROOT "${XECHAIN_SYSROOT}" CACHE PATH
    "Location of the OpenXeChain sysroot (contains bin/clang, lib, ...)" FORCE)

if(NOT EXISTS "${XECHAIN_SYSROOT}/bin/clang")
    message(FATAL_ERROR
        "OpenXeChain sysroot not found at \"${XECHAIN_SYSROOT}\".\n"
        "Build it with the build-toolchain.sh script, or point the toolchain "
        "at it with -DXECHAIN_SYSROOT=<path> or the XECHAIN_ROOT environment variable.")
endif()

# --- Target description ------------------------------------------------------

set(CMAKE_SYSTEM_NAME xbox360)
set(CMAKE_SYSTEM_PROCESSOR ppc32)
set(CMAKE_SYSTEM_VERSION xbox360)

# --- Compilers ----------------------------------------------------------------
# Full paths are used so that clang.cfg/clang++.cfg are picked up automatically.
set(CMAKE_C_COMPILER   "${XECHAIN_SYSROOT}/bin/clang")
set(CMAKE_CXX_COMPILER "${XECHAIN_SYSROOT}/bin/clang++")
set(CMAKE_ASM_COMPILER "${XECHAIN_SYSROOT}/bin/clang")

set(CMAKE_C_COMPILER_TARGET   ppc32-xbox360)
set(CMAKE_CXX_COMPILER_TARGET ppc32-xbox360)
set(CMAKE_ASM_COMPILER_TARGET ppc32-xbox360)

set(CMAKE_SYSROOT "${XECHAIN_SYSROOT}")

# --- LLVM binutils -------------------------------------------------------------

set(CMAKE_AR       "${XECHAIN_SYSROOT}/bin/llvm-ar"     CACHE FILEPATH "OpenXeChain archiver" FORCE)
set(CMAKE_RANLIB   "${XECHAIN_SYSROOT}/bin/llvm-ranlib" CACHE FILEPATH "OpenXeChain ranlib" FORCE)
set(CMAKE_NM       "${XECHAIN_SYSROOT}/bin/llvm-nm"     CACHE FILEPATH "OpenXeChain nm" FORCE)
set(CMAKE_OBJDUMP  "${XECHAIN_SYSROOT}/bin/llvm-objdump" CACHE FILEPATH "OpenXeChain objdump" FORCE)
set(CMAKE_OBJCOPY  "${XECHAIN_SYSROOT}/bin/llvm-objcopy" CACHE FILEPATH "OpenXeChain objcopy" FORCE)
set(CMAKE_STRIP    "${XECHAIN_SYSROOT}/bin/llvm-strip"  CACHE FILEPATH "OpenXeChain strip" FORCE)

# --- Cross-compilation behaviour ------------------------------------------------

# CMake cannot link & run test programs for this target, so it only compiles
# them when probing the compiler.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Only ever search the sysroot for libraries/includes/packages; never the host.
set(CMAKE_FIND_ROOT_PATH
    "${XECHAIN_SYSROOT}"
    "${XECHAIN_SYSROOT}/ppc-xbox360")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# There is no Platform/xbox360.cmake module, so CMake never loads a paths
# module for this system and CMAKE_SYSTEM_PREFIX_PATH ends up empty (that is
# what the repeated "System is unknown to cmake" messages are about).  With no
# prefixes, find_library()/find_path() have nothing to search at all and every
# probe returns NOTFOUND - e.g. Mesa dies with "M_LIB ... set to NOTFOUND".
# Spell out the target directories explicitly instead.  They mirror what
# clang.cfg puts on the command line: Newlib (crt0/libc/libm) lives in
# ppc-xbox360/, while libc++, SDL, Mesa and xecorelib are installed into the
# sysroot prefix itself.
list(APPEND CMAKE_LIBRARY_PATH
    "${XECHAIN_SYSROOT}/ppc-xbox360/lib"
    "${XECHAIN_SYSROOT}/lib")
list(APPEND CMAKE_INCLUDE_PATH
    "${XECHAIN_SYSROOT}/ppc-xbox360/include"
    "${XECHAIN_SYSROOT}/include")
list(APPEND CMAKE_PREFIX_PATH
    "${XECHAIN_SYSROOT}/ppc-xbox360"
    "${XECHAIN_SYSROOT}")

# Never let pkg-config see the host system's package database: the sysroot is
# self-contained.  There are no .pc files in it yet, so host-only dependencies
# (valgrind, libunwind, libdrm, X11, ...) are simply not found, which is the
# desired cross-build behaviour - their headers must not leak into the target.
set(ENV{PKG_CONFIG_LIBDIR} "${XECHAIN_SYSROOT}/ppc-xbox360/lib/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${XECHAIN_SYSROOT}")

# --- Default flags ---------------------------------------------------------------

# Freestanding: the toolchain is a bare-metal-style environment.
set(CMAKE_C_FLAGS_INIT   "-ffreestanding")
set(CMAKE_CXX_FLAGS_INIT "-ffreestanding")

# crt0.o provides _start, which runs the Newlib/global-constructor init and
# then calls main(). Make it the PE entry point explicitly.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,/entry:_start")

# Executables are PE32 files for the "XBOX" subsystem. Convert them to XEX
# with SynthXEX (see the XEX.cmake helper) to run them on a console.
