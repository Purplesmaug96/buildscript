# OpenGL (Mesa softpipe) samples. These render with real GL entry points
# (glClear, glBegin/glVertex/glEnd, glRotatef, ...) via the xbox360 backend:
# a static libxbox360.a built from the Mesa tree (mesa/..) plus its installed
# headers (GL/gl.h, xbox360/xbox360_api.h), which sit in the sysroot after a
# "cmake --install" of the xbox360 Mesa target with
# -DCMAKE_INSTALL_PREFIX=<sysroot>.
#
# If libxbox360.a is not installed yet, this directory is skipped by the
# parent samples/CMakeLists.txt.

add_library(glsample STATIC glsample.c)
target_include_directories(glsample PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}"          # glsample.h
    "${XECHAIN_SYSROOT}/include")        # GL/gl.h, xbox360/xbox360_api.h
target_link_libraries(glsample PRIVATE screen)

# All OpenGL quantities and prototypes come from <GL/gl.h>; the fixed
# pipeline and classic entry points are provided by the xbox360 backend
# (libxbox360.a, located by the parent samples/CMakeLists.txt).
foreach(_sample magenta triangle cube map)
    add_executable(gl_${_sample} ${_sample}.c)
    target_link_libraries(gl_${_sample} PRIVATE glsample
        "${XECHAIN_XBOX360_GL_LIBRARY}")
    openxechain_add_xex(gl_${_sample})
endforeach()