# OpenXeChain samples

Example programs for the OpenXeChain Xbox 360 toolchain, built with the
bundled CMake toolchain file.

## Prerequisites

A built OpenXeChain sysroot, i.e. `<repo>/sysroot` must contain `bin/clang`,
`lib/xecorelib.a`, `lib/generic/libclang_rt.builtins-powerpc.a`, and the
Newlib headers/library. Build it with:

    ./build-toolchain.sh

## Building

    cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/ppc-xbox360-toolchain.cmake -G Ninja
    cmake --build build

Each sample is linked into a PE32 "XBOX" image and then converted to a
runnable XEX by SynthXEX as a post-build step:

    build/samples/hello/hello.xex
    build/samples/magenta/magenta.xex
    build/samples/fileio/fileio.xex
    build/samples/threads/threads.xex
    build/samples/cpp/cpp.xex

The sysroot location is taken from `-DXECHAIN_SYSROOT=<path>`, the
`XECHAIN_ROOT` environment variable, or defaults to `<repo>/sysroot`.

## Running

Copy a `.xex` to a FAT32 USB stick (or an Aurora content path) and launch it
from your dashboard/loader, or rename it to `default.xex` to run it via an
exploit-based loader (BadUpdate, etc.). A signed/patched console (RGH, JTAG,
BadUpdate...) is required.

## The samples

| Sample   | What it demonstrates                                            |
|----------|-----------------------------------------------------------------|
| `hello`  | `printf` / console output. Newlib routes stdout through the kernel debug channel (the same trap `DbgPrint` uses), so it appears on debug consoles / kernel debug output, not on the TV. |
| `magenta`| Framebuffer access. Maps the scanout surface and fills it magenta - the classic "is the framebuffer alive?" test - then draws text on it. |
| `fileio` | File I/O. Newlib stdio (`fopen`/`fwrite`/`fread`) and the raw XAM API (`CreateFileA`/`WriteFile`/`ReadFile`) on the hard drive. Needs an HDD (`Hdd:\`); results are reported via `printf`. |
| `threads`| Kernel threads (`ExCreateThread`), delays (`KeDelayExecutionThread`) and the system clock (`KeQuerySystemTime`). |
| `cpp`    | C++ without a C++ standard library. The sysroot ships no libc++, so use C headers (`<stdio.h>`) and plain C++ features; global constructors work via crt0's `__CTOR_LIST__`. |
| `gl_magenta` | Real OpenGL. Creates an off-screen Mesa softpipe GL context (`xbox360_create`) and clears it to magenta with `glClearColor`/`glClear`, then blits the frame to the scanout surface. |
| `gl_triangle` | OpenGL immediate mode: a white triangle via `glBegin(GL_TRIANGLES)`/`glVertex2f`/`glEnd` on the compatibility-profile context. |
| `gl_cube` | Animated OpenGL: a spinning colour cube driven by `glRotatef`, `glFrustum` projection, depth testing and the kernel system timer. |

### GL samples

The `gl_*` samples need the Mesa xbox360 software backend, built from the
bundled Mesa tree and installed into the sysroot:

    cmake -B mesa/build-cross \
        -DCMAKE_TOOLCHAIN_FILE=cmake/ppc-xbox360-toolchain.cmake \
        -DCMAKE_INSTALL_PREFIX=<sysroot> \
        -DMESA_OP_GALLIUM_DRIVERS="softpipe;xbox360" \
        -DMESA_OP_GLX=disabled -DMESA_OP_EGL=OFF -DMESA_OP_LLVM=disabled \
        -DMESA_OP_SPIRV_TOOLS=disabled -DMESA_OP_ZSTD=disabled
    cmake --build mesa/build-cross --target xbox360
    cmake --install mesa/build-cross

This installs `libxbox360.a`, `GL/gl.h` and `xbox360/xbox360_api.h` into the
sysroot; the GL samples are then picked up automatically by
`samples/CMakeLists.txt`. Without it they are skipped and the other samples
build unchanged.

## Writing your own programs

Point CMake at the toolchain file and link against the same crt0/library
stack the samples use:

    cmake_minimum_required(VERSION 3.20)
    project(MyGame C)
    include(<repo>/cmake/XEX.cmake)          # provides openxechain_add_xex()
    add_executable(mygame main.c)
    openxechain_add_xex(mygame)              # produces mygame.xex

Programs are entered at `main()` (crt0 runs the Newlib init and global
constructors first); returning from `main()` exits back to the dashboard.
The available APIs are declared in `<sysroot>/include/xecore/` -
`xboxkrnl.h` for kernel exports, `xam.h` for XAM exports.

### Notes

- **Console output**: there is no on-screen console in this toolchain.
  `printf` writes to the kernel debug channel (visible on debug consoles /
  XBDM-style debug output). The `magenta` sample shows how to draw text on
  the screen directly.
- **Framebuffer address**: `samples/common/screen.h` documents where the
  scanout surface is looked up (ATI info page at physical `0xEC806100`,
  falling back to `0x1E000000`) and the 32x32-tile swizzle used to draw on
  it. If nothing appears on your hardware, that header is where to look.
- **File paths**: `Hdd:\` requires an internal HDD; `Usb:\` and `Game:\`
  are handled by XAM too.
- **Linking**: the PE image is built for the XBOX subsystem with
  `_start` as the entry point (set in the toolchain file). The
  `lld-link: warning: /align specified without /driver` message is
  cosmetic.
