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
