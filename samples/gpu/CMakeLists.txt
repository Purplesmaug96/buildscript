# GPU dev samples: drive the primary ring buffer / EDRAM directly through the
# xenos backend's dev hooks (libxbox360.a), no GL involved.
#
# The dev hooks (xe_gpu_dev_*) are part of the merged xbox360 archive, so the
# sysroot install of libxbox360.a is required (same as the GL samples).

add_executable(tri_nir tri_nir.c)
target_include_directories(tri_nir PRIVATE "${XECHAIN_SYSROOT}/include")
target_link_libraries(tri_nir PRIVATE screen "${XECHAIN_XBOX360_GL_LIBRARY}")
openxechain_add_xex(tri_nir)