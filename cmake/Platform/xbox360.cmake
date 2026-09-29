# ============================================================================
# CMake platform module for the ppc32-xbox360 target
#
# While it initialises a system, CMake includes
# Platform/${CMAKE_SYSTEM_NAME}.cmake (see
# Modules/CMakeSystemSpecificInformation.cmake).  When that file does not
# exist every configure and every try_compile probe prints
#
#     System is unknown to cmake, create: Platform/xbox360
#
# and none of the platform defaults are loaded - which is also why
# CMAKE_SYSTEM_LIBRARY_PATH/CMAKE_SYSTEM_INCLUDE_PATH stayed empty and
# find_library(M_LIB m) used to return NOTFOUND.
#
# The Xbox 360 is a freestanding console with no operating system, so like
# every other OS-less target we simply inherit Platform/Generic.cmake: it
# declares that the platform cannot build shared libraries and points
# find_*() at /include, /lib and /bin (each re-rooted through
# CMAKE_FIND_ROOT_PATH, i.e. <sysroot>/ppc-xbox360/lib and friends here).
#
# This file is reached through CMAKE_MODULE_PATH, which
# ppc-xbox360-toolchain.cmake adds its own directory to.
# ============================================================================
include(Platform/Generic)

# --- No position-independent code -------------------------------------------
#
# The ppc32-xbox360 backend cannot emit position-independent code: compiling
# a single -fPIC/-fPIE object dies with
#
#     error: assembler label '' can not be undefined
#
# (llvm/lib/MC/WinCOFFObjectWriter.cpp), and there is nothing to be PIC for
# anyway - the console links static PE/XEX archives only.
#
# Blank here where it works for C/CXX, but ASM's language information is
# processed after this file, so the authoritative blanking lives in
# cmake/xbox360-no-pic.cmake (reached through CMAKE_PROJECT_INCLUDE, which
# CMake runs as the last step of every project() command).
set(CMAKE_C_COMPILE_OPTIONS_PIC "")
set(CMAKE_CXX_COMPILE_OPTIONS_PIC "")
set(CMAKE_C_COMPILE_OPTIONS_PIE "")
set(CMAKE_CXX_COMPILE_OPTIONS_PIE "")
