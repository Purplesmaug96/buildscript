#!/usr/bin/env bash

# Build script for the OpenXeChain toolchain project
#
# Copyright (c) 2025 Aiden Isik
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# ANSI colour escape codes
ANSI_RED="\033[31m"
ANSI_GRN="\033[32m"
ANSI_CLR="\033[0m"

# Toolchain name
TOOLCHAIN_NAME="OpenXeChain"
TOOLCHAIN_STEM="${ANSI_GRN}${TOOLCHAIN_NAME}${ANSI_CLR}> "

# This script can be run from any directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

# User-configurable variables
PREFIX="${PREFIX:-${SCRIPT_DIR}/sysroot}" # Sysroot for the toolchain to be installed into
PREFIX="$(realpath -m "${PREFIX}")"
HOST_CC="${HOST_CC:-clang}" # Host compiler to use (MUST BE CLANG)
HOST_CXX="${HOST_CXX:-clang++}"
BUILD_TYPE="${BUILD_TYPE:-Release}" # Debug level to build LLVM in
PARALLEL="${PARALLEL:-$(nproc)}" # Number of parallel make jobs to run
LLVM_LINK_JOBS="${LLVM_LINK_JOBS:-2}" # Max parallel link jobs for LLVM (linking is memory-hungry)
CLEAN="${CLEAN:-0}" # Set to 1 to wipe the build directories before building

# Components to build. Each positional argument names a component to build:
#   ./build-toolchain.sh llvm newlib mesa sdl
# With no arguments (and no COMPONENT variable), all components are built in
# dependency order.  Valid names: llvm xecorelib newlib crt libcxx pthread mesa sdl
if [[ $# -gt 0 ]]; then
    COMPONENTS=("$@")
elif [[ -n "${COMPONENT:-}" ]]; then
    COMPONENTS=("${COMPONENT}")
else
    COMPONENTS=(llvm xecorelib newlib crt libcxx pthread synthxex mesa sdl)
fi

# Static variables
LLVM_TARGET="ppc32-xbox360"
NEWLIB_TARGET="ppc-xbox360"

# Build directories. Every component is built in its own directory so that
# re-running this script only rebuilds what actually changed, instead of
# rebuilding the entire toolchain from scratch every time.
BUILD_DIR="${SCRIPT_DIR}/build"
LLVM_BUILD_DIR="${BUILD_DIR}/llvm"
XECORELIB_BUILD_DIR="${BUILD_DIR}/xecorelib"
XECORELIB_STAGE_DIR="${XECORELIB_BUILD_DIR}/stage"
NEWLIB_BUILD_DIR="${BUILD_DIR}/newlib"
CRT_BUILD_DIR="${BUILD_DIR}/compiler-rt"
LIBCXX_BUILD_DIR="${BUILD_DIR}/libcxx"
LIBCXXABI_BUILD_DIR="${BUILD_DIR}/libcxxabi"
LIBUNWIND_BUILD_DIR="${BUILD_DIR}/libunwind"
SYNTHXEX_BUILD_DIR="${BUILD_DIR}/synthxex"
PTHREAD_BUILD_DIR="${BUILD_DIR}/pthread"
MESA_BUILD_DIR="${BUILD_DIR}/mesa"
SDL_BUILD_DIR="${BUILD_DIR}/sdl"

# Build log path
BUILD_LOG="${SCRIPT_DIR}/build.log"
: > "${BUILD_LOG}" # Delete the old logs, if they exist

echo "This script is deprecated and will be removed eventually; use build-toolchain.py if possible"

# If we fail to build, run this
fail_build()
{
    echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Failed to build! Check build.log.${ANSI_CLR}"
    exit 1
}

# Sanitise the environment for the cross compiler: clang honours
# LIBRARY_PATH/C_INCLUDE_PATH/CPLUS_INCLUDE_PATH even in cross builds, so
# saved copies are kept and restored for the host-only parts.
save_cross_env()
{
    if [[ -z "${CROSS_ENV_SAVED:-}" ]]; then
        OLD_LIBRARY_PATH="${LIBRARY_PATH}"
        OLD_C_INCLUDE_PATH="${C_INCLUDE_PATH}"
        OLD_CPLUS_INCLUDE_PATH="${CPLUS_INCLUDE_PATH}"
        export CROSS_ENV_SAVED=1
    fi
}

clear_cross_env()
{
    save_cross_env
    export LIBRARY_PATH=""
    export C_INCLUDE_PATH=""
    export CPLUS_INCLUDE_PATH=""
}

restore_cross_env()
{
    if [[ "${CROSS_ENV_SAVED:-0}" == "1" ]]; then
        export LIBRARY_PATH="${OLD_LIBRARY_PATH}"
        export C_INCLUDE_PATH="${OLD_C_INCLUDE_PATH}"
        export CPLUS_INCLUDE_PATH="${OLD_CPLUS_INCLUDE_PATH}"
    fi
}

# Check to make sure all required dependencies are installed
check_deps()
{
    MISSING_DEPS=0

    clang --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing clang!${ANSI_CLR}")

    ar --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing binutils!${ANSI_CLR}")

    git --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing git!${ANSI_CLR}")

    cmake --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing cmake!${ANSI_CLR}")

    make --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing make!${ANSI_CLR}")

    ninja --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing ninja!${ANSI_CLR}")

    python3 --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing python3!${ANSI_CLR}")

    bash --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing bash!${ANSI_CLR}")

    bzip2 --help >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing bzip2!${ANSI_CLR}")

    gzip --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing gzip!${ANSI_CLR}")

    grep --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing grep!${ANSI_CLR}")

    xargs --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing findutils!${ANSI_CLR}")

    sed --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing sed!${ANSI_CLR}")

    tar --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing tar!${ANSI_CLR}")

    unzip --help >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing unzip!${ANSI_CLR}")

    zip -v >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing zip!${ANSI_CLR}")

    gawk --version >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing gawk!${ANSI_CLR}")

    # Zlib cannot be checked like this as it has no binaries, but it should have been installed
    # as a dependency of python3

    # Check for pyyaml
    python3 -c "import yaml" >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing python-pyyaml!${ANSI_CLR}")

    # Mesa generates its GLSL C code with mako and refuses to configure
    # without it (mesa/CMakeLists.txt: "Python with mako and yaml modules is
    # required"), so catch it here instead of several components later.
    python3 -c "import mako" >> "${BUILD_LOG}" 2>&1 ||
        (MISSING_DEPS=1 && echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Missing python-mako! (pip install mako)${ANSI_CLR}")

    if [[ ${MISSING_DEPS} -ne 0 ]]; then
        echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Dependencies are missing! Please install them.${ANSI_CLR}"
    fi

    return ${MISSING_DEPS}
}

# ---------------------------------------------------------------------------
# Component: llvm - the cross compiler (clang + lld) itself
# ---------------------------------------------------------------------------
build_llvm()
{
    # Configure it first
    echo -e "${TOOLCHAIN_STEM}Configuring the cross compiler... (this may take a while)"

    LLVM_CMAKE_ARGS=(
        -DCMAKE_C_COMPILER="${HOST_CC}"
        -DCMAKE_CXX_COMPILER="${HOST_CXX}"
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
        -DCMAKE_INSTALL_PREFIX="${PREFIX}"
        -DLLVM_ENABLE_PROJECTS="lld;clang"
        -DLLVM_TARGETS_TO_BUILD=PowerPC
        -DLLVM_DEFAULT_TARGET_TRIPLE="${LLVM_TARGET}"
        -DLLVM_INSTALL_BINUTILS_SYMLINKS=true
        -DLLVM_INSTALL_CCTOOLS_SYMLINKS=true
        -DLLVM_INSTALL_TOOLCHAIN_ONLY=true
        -DLLVM_INCLUDE_TESTS=false
        -DLLVM_INCLUDE_BENCHMARKS=false
        -DLLVM_INCLUDE_EXAMPLES=false
        -DLLVM_INCLUDE_DOCS=false
        -DLLVM_OPTIMIZED_TABLEGEN=true
        -DLLVM_PARALLEL_LINK_JOBS="${LLVM_LINK_JOBS}"
        -DCLANG_ENABLE_STATIC_ANALYZER=false
        -DCLANG_ENABLE_ARCMT=false
        -G "Ninja"
    )

    # Cache compiled objects with ccache if it is installed, to speed up rebuilds
    if command -v ccache > /dev/null 2>&1; then
        LLVM_CMAKE_ARGS+=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
    fi

    # Link with lld if it is installed, as it is much faster than the default linker
    if command -v ld.lld > /dev/null 2>&1; then
        LLVM_CMAKE_ARGS+=(-DLLVM_USE_LINKER=lld)
    fi

    cmake -S "${SCRIPT_DIR}/llvm/llvm" -B "${LLVM_BUILD_DIR}" "${LLVM_CMAKE_ARGS[@]}" >> "${BUILD_LOG}" 2>&1 || fail_build

    # Now build and install
    echo -e "${TOOLCHAIN_STEM}Building the cross compiler... (this may take a WHILE)"
    ninja -C "${LLVM_BUILD_DIR}" -j"${PARALLEL}" >> "${BUILD_LOG}" 2>&1 || fail_build

    echo -e "${TOOLCHAIN_STEM}Installing the cross compiler... (this may take a while)"
    ninja -C "${LLVM_BUILD_DIR}" install >> "${BUILD_LOG}" 2>&1 || fail_build

    echo -e "${TOOLCHAIN_STEM}Cross compiler built and installed!"

    # Define the default command line flags to be used by the cross-compiler.
    # Linkage to Newlib/xecorelib is also added to these files, but after it's been built and installed.
    echo -e "${TOOLCHAIN_STEM}Writing initial Clang configuration scripts."

    cat > "${PREFIX}/bin/clang.cfg" << EOF
-Wno-main-return-type
--sysroot=<CFGDIR>/..
--rtlib=compiler-rt
-fdeclspec
-mlongcall
EOF

    cat > "${PREFIX}/bin/clang++.cfg" << EOF
-Wno-main-return-type
--sysroot=<CFGDIR>/..
--rtlib=compiler-rt
-fdeclspec
-mlongcall
EOF

    # Clear the environment variables the C/C++ compiler is sensitive to,
    # to avoid pollution. We restore them for the host-side components.
    clear_cross_env
}

# ---------------------------------------------------------------------------
# Component: xecorelib - the Xbox 360 kernel/XAM import libraries
# ---------------------------------------------------------------------------
build_xecorelib()
{
    # Build xecorelib
    echo -e "${TOOLCHAIN_STEM}Building and installing xecorelib."

    cd "${XECORELIB_BUILD_DIR}" || fail_build

    # Run the xecorelib build script
    PREFIX="${PREFIX}" bash "${SCRIPT_DIR}/xecorelib/install.sh" >> "${BUILD_LOG}" 2>&1 || fail_build

    # Also install to a staging directory, to build Newlib with it
    BINDIR="${PREFIX}/bin" PREFIX="${XECORELIB_STAGE_DIR}" bash "${SCRIPT_DIR}/xecorelib/install.sh" >> "${BUILD_LOG}" 2>&1 || fail_build
    echo -e "${TOOLCHAIN_STEM}Built and installed xecorelib!"

    cd "${SCRIPT_DIR}" || fail_build
}

# ---------------------------------------------------------------------------
# Component: newlib - the libc for the console
# ---------------------------------------------------------------------------
build_newlib()
{
    # Build the Newlib libc
    echo -e "${TOOLCHAIN_STEM}Getting ready to build the Newlib C library."

    cd "${NEWLIB_BUILD_DIR}" || fail_build

    # Configure Newlib
    echo -e "${TOOLCHAIN_STEM}Configuring the Newlib C library... (this may take a while)"

    "${SCRIPT_DIR}/newlib/newlib/configure" \
        CC="${PREFIX}/bin/clang -nostdlib -I${XECORELIB_STAGE_DIR}/include" \
        CPP="${PREFIX}/bin/clang-cpp" \
        LD="${PREFIX}/bin/lld-link" \
        AR="${PREFIX}/bin/llvm-ar" \
        AS="${PREFIX}/bin/llvm-as" \
        STRIP="${PREFIX}/bin/llvm-strip" \
        RANLIB="${PREFIX}/bin/llvm-ranlib" \
        --prefix="${PREFIX}" \
        --host="${NEWLIB_TARGET}" \
        --target="${NEWLIB_TARGET}" \
        --enable-newlib-supplied-syscalls=yes \
        --enable-newlib-mb \
        --enable-newlib-iconv \
        >> "${BUILD_LOG}" 2>&1 || fail_build

    # Now build and install
    echo -e "${TOOLCHAIN_STEM}Building the Newlib C library..."
    make -j"${PARALLEL}" >> "${BUILD_LOG}" 2>&1 || fail_build

    echo -e "${TOOLCHAIN_STEM}Installing the Newlib C library..."
    make install >> "${BUILD_LOG}" 2>&1 || fail_build

    cd "${SCRIPT_DIR}" || fail_build

    # Add Newlib/xecorelib linkage to the default compiler flags now that it is installed
    echo -e "${TOOLCHAIN_STEM}Adding Newlib linkage to Clang configuration scripts."

    cat >> "${PREFIX}/bin/clang.cfg" << EOF
-isystem <CFGDIR>/../${NEWLIB_TARGET}/include
-isystem <CFGDIR>/../include
-Wl,/libpath:<CFGDIR>/../${NEWLIB_TARGET}/lib,/libpath:<CFGDIR>/../lib
-Wl,/defaultlib:xecorelib.a,/defaultlib:libc.a
EOF

    cat >> "${PREFIX}/bin/clang++.cfg" << EOF
-isystem <CFGDIR>/../${NEWLIB_TARGET}/include
-isystem <CFGDIR>/../include
-Wl,/libpath:<CFGDIR>/../${NEWLIB_TARGET}/lib,/libpath:<CFGDIR>/../lib
-Wl,/defaultlib:xecorelib.a,/defaultlib:libc.a
EOF

    echo -e "${TOOLCHAIN_STEM}Newlib C library built and installed!"
}

# ---------------------------------------------------------------------------
# Component: crt - compiler-rt builtins
# ---------------------------------------------------------------------------
build_crt()
{
    # Configure compiler-rt
    # We need to override the compiler checks, otherwise CMake will attempt to
    # build test programs, which won't work, as it'll try to link compiler-rt,
    # which is not yet installed.
    # LLVM package discovery is disabled, as compiler-rt would pick up the host's
    # LLVM (or its own LTO shared library from the build tree), which cannot be
    # imported on a target platform with no dynamic linking support.
    echo -e "${TOOLCHAIN_STEM}Configuring compiler-rt..."
    cmake -S "${SCRIPT_DIR}/llvm/compiler-rt" -B "${CRT_BUILD_DIR}" \
          -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
          -DCMAKE_SYSTEM_NAME="Generic" \
          -DCMAKE_CROSSCOMPILING=true \
          -DCMAKE_C_COMPILER="${PREFIX}/bin/clang" \
          -DCMAKE_CXX_COMPILER="${PREFIX}/bin/clang++" \
          -DCMAKE_AR="${PREFIX}/bin/llvm-ar" \
          -DCMAKE_LINKER="${PREFIX}/bin/lld-link" \
          -DCMAKE_RANLIB="${PREFIX}/bin/llvm-ranlib" \
          -DCMAKE_SYSROOT="${PREFIX}" \
          -DCMAKE_C_COMPILER_WORKS=true \
          -DCMAKE_CXX_COMPILER_WORKS=true \
          -DCMAKE_C_COMPILER_TARGET="ppc32-xbox360" \
          -DCOMPILER_RT_BUILD_BUILTINS=true \
          -DCOMPILER_RT_DEFAULT_TARGET_ONLY=true \
          -DCOMPILER_RT_BUILD_SANITIZERS=false \
          -DCOMPILER_RT_BUILD_XRAY=false \
          -DCOMPILER_RT_BUILD_LIBFUZZER=false \
          -DCOMPILER_RT_BUILD_PROFILE=false \
          -DCOMPILER_RT_STANDALONE_BUILD=true \
          -DCOMPILER_RT_BUILTINS_ENABLE_PIC=false \
          -DCOMPILER_RT_EXCLUDE_ATOMIC_BUILTIN=false \
          -DCOMPILER_RT_BAREMETAL_BUILD=true \
          -DCOMPILER_RT_INCLUDE_TESTS=false \
          -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=true \
          -G "Ninja" >> "${BUILD_LOG}" 2>&1 || fail_build

    # Now build and install
    echo -e "${TOOLCHAIN_STEM}Building compiler-rt..."
    ninja -C "${CRT_BUILD_DIR}" -j"${PARALLEL}" >> "${BUILD_LOG}" 2>&1 || fail_build

    echo -e "${TOOLCHAIN_STEM}Installing compiler-rt..."
    ninja -C "${CRT_BUILD_DIR}" install >> "${BUILD_LOG}" 2>&1 || fail_build

    # Add compiler-rt linkage to Clang config scripts
    echo -e "${TOOLCHAIN_STEM}Adding compiler-rt linkage to Clang configuration scripts."

    cat >> "${PREFIX}/bin/clang.cfg" << EOF
-Wl,/libpath:<CFGDIR>/../lib/generic
-Wl,/defaultlib:libclang_rt.builtins-powerpc.a
EOF

    cat >> "${PREFIX}/bin/clang++.cfg" << EOF
-Wl,/libpath:<CFGDIR>/../lib/generic
-Wl,/defaultlib:libclang_rt.builtins-powerpc.a
EOF

    echo -e "${TOOLCHAIN_STEM}Compiler-rt built and installed!"

    # The cross-compiled components are done; let the host see its own environment again.
    restore_cross_env
}

# ---------------------------------------------------------------------------
# Component: libcxx - C++ standard library (libc++ + libc++abi + libunwind)
#
# Mesa's GLSL front-end and many other components are C++, so the console
# needs a C++ standard library: LLVM's libc++, its ABI library (exceptions
# and RTTI) and Itanium unwinding entry points.  LLVM's own libunwind cannot
# be built (the console emits COFF objects, libunwind only supports ELF
# targets), so a small stub implementation ships instead - see
# "${SCRIPT_DIR}/unwind/unwind-stubs.c".
# ---------------------------------------------------------------------------
setup_libcxx()
{
    local libcxx_lib="${PREFIX}/lib/libc++.a"
    local libcxxabi_lib="${PREFIX}/lib/libc++abi.a"
    local libunwind_lib="${PREFIX}/${NEWLIB_TARGET}/lib/libunwind.a"
    local cxx_include="${PREFIX}/include/c++/v1"

    # The pthread component runs *after* libcxx in COMPONENTS, but the
    # -D_POSIX_* feature macros it writes into the Clang config scripts are
    # needed to even compile libc++: Newlib's <pthread.h> hides every
    # prototype behind _POSIX_THREADS, and chrono.cpp needs _POSIX_TIMERS for
    # clock_gettime (else: "Monotonic clock not implemented on this
    # platform").  Write them here so the config is right no matter what
    # order the components are built in.  Only the macros - the
    # -Wl,/defaultlib: lines stay with the pthread component so the static
    # link order is unchanged.  Idempotent.
    setup_pthread_cfg_macros

    # Wire the C++ library into the Clang config scripts as early as
    # possible: clang++ configs are applied in the same order as the files
    # on disk, so the libc++ include dir must come *before* the Newlib
    # dirs.  Config-file flags are processed before any command-line flags,
    # so without this the Newlib <ctype.h> would shadow libc++'s.
    for cfg in "${PREFIX}/bin/clang.cfg" "${PREFIX}/bin/clang++.cfg"; do
        if ! grep -q "include/c++/v1" "${cfg}" 2>/dev/null; then
            sed -i '1i -isystem <CFGDIR>/../include/c++/v1' "${cfg}"
        fi
        if ! grep -q "stdlib=libc++" "${cfg}" 2>/dev/null; then
            cat >> "${cfg}" << EOF
-stdlib=libc++
-Wl,/defaultlib:libc++.a,/defaultlib:libc++abi.a,/defaultlib:libunwind.a
EOF
        fi
    done

    # The unwinding stubs replace LLVM's libunwind: they compile with the
    # console toolchain without any configuration.
    if [[ ! -f "${libunwind_lib}" ]] ||
       find "${SCRIPT_DIR}/unwind" -name '*.c' -newer "${libunwind_lib}" | grep -q .; then
        echo -e "${TOOLCHAIN_STEM}Building unwinding stubs..."

        "${PREFIX}/bin/clang" --target="${LLVM_TARGET}" \
            -c -O2 -ffreestanding \
            "${SCRIPT_DIR}/unwind/unwind-stubs.c" \
            -o "${BUILD_DIR}/unwind-stubs.o" >> "${BUILD_LOG}" 2>&1 || fail_build
        "${PREFIX}/bin/llvm-ar" rcs "${libunwind_lib}" "${BUILD_DIR}/unwind-stubs.o" \
            >> "${BUILD_LOG}" 2>&1 || fail_build
        "${PREFIX}/bin/llvm-ranlib" "${libunwind_lib}" >> "${BUILD_LOG}" 2>&1 \
            || fail_build
    fi

    if [[ -f "${libcxxabi_lib}" && -f "${libcxx_lib}" && -f "${cxx_include}/string" ]] &&
       grep -q "stdlib=libc++" "${PREFIX}/bin/clang.cfg" 2>/dev/null; then
        return 0
    fi

    echo -e "${TOOLCHAIN_STEM}Configuring libc++..."

    cmake -S "${SCRIPT_DIR}/llvm/libcxx" -B "${LIBCXX_BUILD_DIR}" \
          -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/ppc-xbox360-toolchain.cmake" \
          -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
          -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
          -DLIBCXX_ENABLE_SHARED=OFF \
          -DLIBCXX_ENABLE_STATIC=ON \
          -DLIBCXX_USE_COMPILER_RT=ON \
          -DLIBCXX_ENABLE_THREADS=ON \
          -DLIBCXX_HAS_PTHREAD_API=ON \
          -DLIBCXX_ENABLE_MONOTONIC_CLOCK=ON \
          -DLIBCXX_CXX_ABI=libcxxabi \
          -DLIBCXX_CXX_ABI_INCLUDE_PATHS="${SCRIPT_DIR}/llvm/libcxxabi/include" \
          -DCMAKE_CXX_FLAGS="-ffreestanding -isystem ${SCRIPT_DIR}/llvm/libcxxabi/include" \
          -DLIBCXX_INCLUDE_TESTS=OFF \
          -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
          -DLIBCXX_ABI_VERSION=2 \
          -G "Ninja" >> "${BUILD_LOG}" 2>&1 || fail_build

    ninja -C "${LIBCXX_BUILD_DIR}" -j"${PARALLEL}" >> "${BUILD_LOG}" 2>&1 || fail_build

    # The IWYU mapping file is generated by a ninja dependency, but that can
    # race with the install step on this fast machine.  Generate it up front
    # if it is missing.
    if [[ ! -f "${LIBCXX_BUILD_DIR}/include/c++/v1/libcxx.imp" ]]; then
        python3 "${SCRIPT_DIR}/llvm/libcxx/utils/generate_iwyu_mapping.py" \
            -o "${LIBCXX_BUILD_DIR}/include/c++/v1/libcxx.imp" \
            >> "${BUILD_LOG}" 2>&1 || fail_build
    fi

    ninja -C "${LIBCXX_BUILD_DIR}" install >> "${BUILD_LOG}" 2>&1 || fail_build

    echo -e "${TOOLCHAIN_STEM}Configuring libc++abi..."

    cmake -S "${SCRIPT_DIR}/llvm/libcxxabi" -B "${LIBCXXABI_BUILD_DIR}" \
          -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/ppc-xbox360-toolchain.cmake" \
          -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
          -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
          -DLIBCXXABI_ENABLE_SHARED=OFF \
          -DLIBCXXABI_ENABLE_STATIC=ON \
          -DLIBCXXABI_USE_COMPILER_RT=ON \
          -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
          -DLIBCXXABI_INCLUDE_TESTS=OFF \
          -DLIBCXXABI_LIBCXX_INCLUDES="${PREFIX}/include/c++/v1" \
          -DCMAKE_CXX_FLAGS="-ffreestanding -isystem ${PREFIX}/include/c++/v1" \
          -G "Ninja" >> "${BUILD_LOG}" 2>&1 || fail_build

    ninja -C "${LIBCXXABI_BUILD_DIR}" -j"${PARALLEL}" >> "${BUILD_LOG}" 2>&1 || fail_build
    ninja -C "${LIBCXXABI_BUILD_DIR}" install >> "${BUILD_LOG}" 2>&1 || fail_build

    # Wire the C++ standard library into the Clang config scripts (already
    # done above - keep this comment for the reader).
    for cfg in "${PREFIX}/bin/clang.cfg" "${PREFIX}/bin/clang++.cfg"; do
        : # done at the top of this function
    done

    echo -e "${TOOLCHAIN_STEM}C++ standard library built and installed!"
}

build_libcxx()
{
    setup_libcxx
}

# ---------------------------------------------------------------------------
# Component: synthxex - host-side XEX packager
# ---------------------------------------------------------------------------
build_synthxex()
{
    # Build SynthXEX
    echo -e "${TOOLCHAIN_STEM}Getting ready to build SynthXEX."

    # Configure SynthXEX
    echo -e "${TOOLCHAIN_STEM}Configuring SynthXEX..."

    cmake -S "${SCRIPT_DIR}/synthxex" -B "${SYNTHXEX_BUILD_DIR}" \
          -DCMAKE_C_COMPILER="${HOST_CC}" \
          -DCMAKE_CXX_COMPILER="${HOST_CXX}" \
          -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
          -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
          -G "Ninja" >> "${BUILD_LOG}" 2>&1 || fail_build

    # Now build and install
    echo -e "${TOOLCHAIN_STEM}Building SynthXEX..."
    ninja -C "${SYNTHXEX_BUILD_DIR}" -j"${PARALLEL}" >> "${BUILD_LOG}" 2>&1 || fail_build

    echo -e "${TOOLCHAIN_STEM}Installing SynthXEX..."
    ninja -C "${SYNTHXEX_BUILD_DIR}" install >> "${BUILD_LOG}" 2>&1 || fail_build

    echo -e "${TOOLCHAIN_STEM}SynthXEX built and installed!"
}

# ---------------------------------------------------------------------------
# Component: pthread - single-threaded POSIX threads shim
#
# The Xbox 360 kernel has no thread API for games, so threads cannot be
# spawned from C.  This shim satisfies the pthread symbols Mesa references
# at link time (its c11 threads wrapper and util code compile against
# full POSIX).  It also defines the POSIX feature macros Newlib needs to
# declare the pthread types/functions, and links libpthread.a into every
# image via the Clang config scripts - exactly what the Mesa CMake port
# expects from "the toolchain" (see cmake/mesa-checks.cmake).
# ---------------------------------------------------------------------------
# The POSIX feature macros Newlib's <pthread.h> and libc++'s chrono.cpp need
# to see the pthread prototypes and clock_gettime().  These are plain -D
# flags with no dependency on the shim library, so they are kept separate
# from the link line: setup_libcxx() writes them before libc++ is compiled
# (the pthread component runs after libcxx), while -Wl,/defaultlib: must stay
# where the pthread component puts it.  Idempotent across re-runs.
setup_pthread_cfg_macros()
{
    for cfg in "${PREFIX}/bin/clang.cfg" "${PREFIX}/bin/clang++.cfg"; do
        if ! grep -q -e "-D_POSIX_THREADS" "${cfg}" 2>/dev/null; then
            cat >> "${cfg}" << EOF
-D_POSIX_THREADS
-D_POSIX_BARRIERS
-D_POSIX_READER_WRITER_LOCKS
-D_POSIX_TIMEOUTS
-D_UNIX98_THREAD_MUTEX_ATTRIBUTES
-D_POSIX_CLOCK_SELECTION
-D_POSIX_MONOTONIC_CLOCK
-D_POSIX_TIMERS
EOF
        fi
    done
}

setup_pthread_shim()
{
    local pthread_lib="${PREFIX}/${NEWLIB_TARGET}/lib/libpthread.a"
    local cfg_line="libpthread.a"

    # Compile and archive the shim if it is stale or missing.  The shim is
    # tiny, so recompiling it whenever it changed under the source dir is fine.
    if [[ ! -f "${pthread_lib}" ]] ||
       find "${SCRIPT_DIR}/pthread" -name '*.c' -newer "${pthread_lib}" | grep -q . ||
       find "${SCRIPT_DIR}/pthread" -name '*.S' -newer "${pthread_lib}" | grep -q .; then
        echo -e "${TOOLCHAIN_STEM}Building the single-threaded pthread shim."

        rm -f "${PTHREAD_BUILD_DIR}"/*.o
        for src in "${SCRIPT_DIR}"/pthread/*.c; do
            "${PREFIX}/bin/clang" -c -O2 -ffreestanding -I"${SCRIPT_DIR}/pthread" \
                "${src}" \
                -o "${PTHREAD_BUILD_DIR}/$(basename "${src}" .c).o" \
                >> "${BUILD_LOG}" 2>&1 || fail_build
        done
        for src in "${SCRIPT_DIR}"/pthread/*.S; do
            "${PREFIX}/bin/clang" -c -O2 -ffreestanding -I"${SCRIPT_DIR}/pthread" \
                "${src}" \
                -o "${PTHREAD_BUILD_DIR}/$(basename "${src}" .S).o" \
                >> "${BUILD_LOG}" 2>&1 || fail_build
        done
        "${PREFIX}/bin/llvm-ar" rcs "${pthread_lib}" "${PTHREAD_BUILD_DIR}"/*.o \
            >> "${BUILD_LOG}" 2>&1 || fail_build
        "${PREFIX}/bin/llvm-ranlib" "${pthread_lib}" >> "${BUILD_LOG}" 2>&1 \
            || fail_build

        # Install the headers the shim supplies on top of Newlib's (syslog).
        if [[ -f "${SCRIPT_DIR}/pthread/syslog.h" ]]; then
            install -m 644 "${SCRIPT_DIR}/pthread/syslog.h" \
                "${PREFIX}/${NEWLIB_TARGET}/include/syslog.h"
        fi
    fi

    # The feature macros first (setup_libcxx() may already have written
    # them), then the libpthread/libm default link.  Idempotent across
    # re-runs.
    setup_pthread_cfg_macros
    for cfg in "${PREFIX}/bin/clang.cfg" "${PREFIX}/bin/clang++.cfg"; do
        if ! grep -q "${cfg_line}" "${cfg}" 2>/dev/null; then
            cat >> "${cfg}" << EOF
-Wl,/defaultlib:${cfg_line}
EOF
        fi
        if ! grep -q "defaultlib:libm.a" "${cfg}" 2>/dev/null; then
            cat >> "${cfg}" << EOF
-Wl,/defaultlib:libm.a
EOF
        fi
    done
}

build_pthread()
{
    setup_pthread_shim
    echo -e "${TOOLCHAIN_STEM}pthread shim built and installed!"
}

# ---------------------------------------------------------------------------
# Component: mesa - the xbox360 (softpipe) GL backend, cross-compiled for the
# console and installed into the sysroot (libxbox360.a, GL/gl.h,
# xbox360/xbox360_api.h).  This is what enables the samples/gl_* programs.
# ---------------------------------------------------------------------------
build_mesa()
{
    echo -e "${TOOLCHAIN_STEM}Getting ready to build Mesa (xbox360 backend)."

    # Mesa's GL stack is C++ (the GLSL front-end), so install the C++ standard
    # library first (no-op if it is already installed).
    setup_libcxx
    setup_pthread_shim

    # Mesa targets the console, so the cross compiler's environment must be
    # clean too (clang honours LIBRARY_PATH/C_INCLUDE_PATH even when the
    # target is ppc32-xbox360).
    clear_cross_env

    echo -e "${TOOLCHAIN_STEM}Configuring Mesa..."
    cmake -S "${SCRIPT_DIR}/mesa" -B "${MESA_BUILD_DIR}" \
          -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/ppc-xbox360-toolchain.cmake" \
          -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
          -DXECHAIN_SYSROOT="${PREFIX}" \
          -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
          -DMESA_OP_GALLIUM_DRIVERS="softpipe;xbox360" \
          -DMESA_OP_XBOX360_XENOS="${MESA_OP_XBOX360_XENOS:-ON}" \
          -DMESA_OP_GLX=disabled \
          -DMESA_OP_EGL=OFF \
          -DMESA_OP_LLVM=disabled \
          -DMESA_OP_SPIRV_TOOLS=disabled \
          -DMESA_OP_ZSTD=disabled \
          -DX360_RING_REV="${X360_RING_REV:-OFF}" \
          -DX360_RING_ROT="${X360_RING_ROT:-0}" \
          -DX360_CPU_PROBE_VSX="${X360_CPU_PROBE_VSX:-OFF}" \
          -G "Ninja" >> "${BUILD_LOG}" 2>&1 || fail_build

    # Build and install
    echo -e "${TOOLCHAIN_STEM}Building Mesa..."
    cmake --build "${MESA_BUILD_DIR}" --target xbox360_merged -- -j"${PARALLEL}" >> "${BUILD_LOG}" 2>&1 || fail_build
    echo -e "${TOOLCHAIN_STEM}Installing Mesa..."
    cmake --install "${MESA_BUILD_DIR}" >> "${BUILD_LOG}" 2>&1 || fail_build

    echo -e "${TOOLCHAIN_STEM}Mesa built and installed!"
}

# ---------------------------------------------------------------------------
# Component: sdl - SDL3, cross-compiled for the console
#
# SDL3's SDL_CreateWindow/SDL_GL_CreateContext/SDL_Renderer give homebrew a
# common API; the bundled xbox360 video driver wraps the scanout + Mesa
# (libxbox360.a) backend and the xbox360 timer backend uses KeQuerySystemTime.
# Only the video (xbox360 driver), renderer (software) and timers subsystems
# are enabled; audio, input, camera, sensor, power, GPU and the rest are out
# of scope for now.  The static library is installed into the sysroot
# (lib/libSDL3.a) with its headers, so samples link -lSDL3 along with
# -lxbox360 and -lxecorelib (the latter two come from the .cfg files).
# ---------------------------------------------------------------------------
build_sdl()
{
    echo -e "${TOOLCHAIN_STEM}Getting ready to build SDL3 (xbox360 driver)."

    # SDL3 targets the console, so the cross compiler's environment must be
    # clean too (clang honours LIBRARY_PATH/C_INCLUDE_PATH even when the
    # target is ppc32-xbox360).
    clear_cross_env

    echo -e "${TOOLCHAIN_STEM}Configuring SDL3..."
    cmake -S "${SCRIPT_DIR}/sdl" -B "${SDL_BUILD_DIR}" \
          -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/ppc-xbox360-toolchain.cmake" \
          -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
          -DXECHAIN_SYSROOT="${PREFIX}" \
          -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
          -DSDL_SHARED=OFF \
          -DSDL_STATIC=ON \
          -DSDL_TEST_LIBRARY=OFF \
          -DSDL_EXAMPLES=OFF \
          -DSDL_AUDIO=ON \
          -DSDL_DUMMYAUDIO=OFF \
          -DSDL_DISKAUDIO=OFF \
          -DSDL_VIDEO=ON \
          -DSDL_RENDER=ON \
          -DSDL_JOYSTICK=OFF \
          -DSDL_HAPTIC=OFF \
          -DSDL_HIDAPI=OFF \
          -DSDL_CAMERA=OFF \
          -DSDL_SENSOR=OFF \
          -DSDL_POWER=OFF \
          -DSDL_GPU=OFF \
          -G "Ninja" >> "${BUILD_LOG}" 2>&1 || fail_build

    # Build and install
    echo -e "${TOOLCHAIN_STEM}Building SDL3..."
    ninja -C "${SDL_BUILD_DIR}" -j"${PARALLEL}" >> "${BUILD_LOG}" 2>&1 || fail_build

    echo -e "${TOOLCHAIN_STEM}Installing SDL3..."
    ninja -C "${SDL_BUILD_DIR}" install >> "${BUILD_LOG}" 2>&1 || fail_build

    echo -e "${TOOLCHAIN_STEM}SDL3 built and installed!"
}

if [[ ! -d "newlib" || ! -d "llvm" || ! -d "synthxex" || ! -d "xecorelib" || ! -d "mesa" || ! -d "sdl" ]]; then
    echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Submodules are missing! Please re-clone this repository with --recursive.${ANSI_CLR}"
    fail_build
fi

if [[ "${CLEAN}" == "1" ]]; then
    echo -e "${TOOLCHAIN_STEM}Wiping build directories."
    rm -rf "${BUILD_DIR}"
fi

# Create the sysroot and build directories, if they don't already exist
echo -e "${TOOLCHAIN_STEM}Creating sysroot directory \"${PREFIX}\"."
mkdir -p "${PREFIX}" \
         "${LLVM_BUILD_DIR}" \
         "${XECORELIB_BUILD_DIR}" "${XECORELIB_STAGE_DIR}" \
         "${NEWLIB_BUILD_DIR}" \
         "${CRT_BUILD_DIR}" \
         "${LIBCXX_BUILD_DIR}" "${LIBCXXABI_BUILD_DIR}" "${LIBUNWIND_BUILD_DIR}" \
         "${SYNTHXEX_BUILD_DIR}" \
         "${PTHREAD_BUILD_DIR}" \
         "${MESA_BUILD_DIR}" \
         "${SDL_BUILD_DIR}" >> "${BUILD_LOG}" 2>&1 || fail_build

# Make sure all required dependencies are installed
echo -e "${TOOLCHAIN_STEM}Checking if required dependencies are installed."
check_deps || fail_build

# Build the requested components, in order
for _component in "${COMPONENTS[@]}"; do
    case "${_component}" in
        llvm)      build_llvm ;;
        xecorelib) build_xecorelib ;;
        newlib)    build_newlib ;;
        crt)       build_crt ;;
        libcxx)    build_libcxx ;;
        pthread)   build_pthread ;;
        synthxex)  build_synthxex ;;
        mesa)      build_mesa ;;
        sdl)       build_sdl ;;
        *)
            echo -e "${TOOLCHAIN_STEM}${ANSI_RED}Unknown component \"${_component}\"! Valid components: llvm xecorelib newlib crt libcxx pthread synthxex mesa sdl${ANSI_CLR}"
            fail_build
            ;;
    esac
done

# Finished
echo -e "${TOOLCHAIN_STEM}${ANSI_GRN}${TOOLCHAIN_NAME} has been built successfully.${ANSI_CLR}"
echo -e "${TOOLCHAIN_STEM}${ANSI_GRN}Current installled location: ${PREFIX}.${ANSI_CLR}"
echo -e "${TOOLCHAIN_STEM}${ANSI_GRN}This toolchain is portable, it can be moved to any path and still work.${ANSI_CLR}"
echo -e "${TOOLCHAIN_STEM}${ANSI_GRN}Ensure C_INCLUDE_PATH, CPLUS_INCLUDE_PATH, and LIBRARY_PATH${ANSI_CLR}"
echo -e "${TOOLCHAIN_STEM}${ANSI_GRN}are set blank when using the toolchain, or you may encounter${ANSI_CLR}"
echo -e "${TOOLCHAIN_STEM}${ANSI_GRN}interference from the host libraries.${ANSI_CLR}"
echo -e "${TOOLCHAIN_STEM}${ANSI_GRN}Build directories are kept in ${BUILD_DIR}, so re-running this${ANSI_CLR}"
echo -e "${TOOLCHAIN_STEM}${ANSI_GRN}script will only rebuild what changed. Set CLEAN=1 to wipe them.${ANSI_CLR}"