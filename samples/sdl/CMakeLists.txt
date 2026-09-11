# SDL3 samples. These link against the SDL3 static library built from the
# bundled SDL3 source (sdl/) and installed into the sysroot by
# build-toolchain.sh ("sdl" component): lib/libSDL3.a and its headers under
# include/SDL3.  The xbox360 SDL3 driver references the Mesa GL backend
# (libxbox360.a) for the GL samples, so it must come after libSDL3.a on the
# link line.
#
# If libSDL3.a is not installed yet, this directory is skipped by the parent
# samples/CMakeLists.txt.

foreach(_sample magenta triangle cube texture)
    add_executable(sdl_${_sample} sdl_${_sample}.c)
    target_include_directories(sdl_${_sample} PRIVATE
        "${XECHAIN_SYSROOT}/include")        # SDL3/SDL.h, GL/gl.h, xecore/...
    target_link_libraries(sdl_${_sample} PRIVATE
        "${XECHAIN_SDL3_LIBRARY}"
        "${XECHAIN_XBOX360_GL_LIBRARY}")
    openxechain_add_xex(sdl_${_sample})
endforeach()

# Software renderer demo: uses SDL's built-in software renderer, but SDL3's
# xbox360 video driver still pulls in the GL backend symbols (the driver is a
# single static library), so libxbox360.a must be linked here too.
add_executable(sdl_renderer sdl_renderer.c)
target_include_directories(sdl_renderer PRIVATE "${XECHAIN_SYSROOT}/include")
target_link_libraries(sdl_renderer PRIVATE
    "${XECHAIN_SDL3_LIBRARY}"
    "${XECHAIN_XBOX360_GL_LIBRARY}")
openxechain_add_xex(sdl_renderer)