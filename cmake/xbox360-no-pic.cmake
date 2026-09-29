# ============================================================================
# Force off position-independent code / position-independent executables for
# the ppc32-xbox360 target.
#
# Included by CMake at the end of *every* project() call, driven by
# CMAKE_PROJECT_INCLUDE in ppc-xbox360-toolchain.cmake.
#
# Why this file exists
# --------------------
# Compiler/Clang.cmake pulls in Compiler/GNU, whose __compiler_gnu() macro
# stamps
#
#     CMAKE_<lang>_COMPILE_OPTIONS_PIC = "-fPIC"
#     CMAKE_<lang>_COMPILE_OPTIONS_PIE = "-fPIE"
#
# into every language.  The ppc32-xbox360 backend in LLVM cannot emit either
# form of position-independent code; compiling a single such object already
# fails with
#
#     error: assembler label '' can not be undefined
#
# (llvm/lib/MC/WinCOFFObjectWriter.cpp).  Both spellings hit it: -fPIC on
# libraries marked POSITION_INDEPENDENT_CODE, -fPIE on executables marked
# POSITION_INDEPENDENT_CODE (CMake adds -fPIE to EXE targets unconditionally
# when CMP0083 is OLD, so no check_pie_supported() call is needed to trip
# over it).
#
# There is nothing to gain from PIC here anyway: the console links static
# PE/XEX archives, and Platform/Generic.cmake - which Platform/xbox360.cmake
# inherits - declares that the platform cannot build shared libraries at all.
#
# Why CMAKE_PROJECT_INCLUDE
# -------------------------
# The value cannot be set from the toolchain file: that runs *before*
# Compiler/GNU, which would simply overwrite it with -fPIC again.  Blanking
# it in the Platform module works for C and CXX but not for ASM, whose
# language information is processed later.  CMake documents
# CMAKE_PROJECT_INCLUDE as "included as the last step of all project()
# command calls", i.e. after every enabled language has been determined -
# the only place where all of these are guaranteed to stick.  It is
# idempotent, so repeated project() calls (or try_compile sub-projects, which
# re-read the toolchain file) are fine.
# ============================================================================

foreach(_x360_lang C CXX ASM)
    set(CMAKE_${_x360_lang}_COMPILE_OPTIONS_PIC "")
    set(CMAKE_${_x360_lang}_COMPILE_OPTIONS_PIE "")
    set(CMAKE_${_x360_lang}_LINK_OPTIONS_PIE "")
endforeach()
unset(_x360_lang)
