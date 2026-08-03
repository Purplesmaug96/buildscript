# ============================================================================
# XEX.cmake - helpers for producing Xbox 360 executables (.xex)
#
# After an executable has been linked (as a PE32 "XBOX" image) this module
# converts it into an XEX2 file with SynthXEX, which ships inside the
# OpenXeChain sysroot.
#
# Provided functions:
#   openxechain_add_xex(<target>
#                       [TYPE title|titledll|sysdll|dll]
#                       [OUTPUT <path/to/result.xex>])
#
# A POST_BUILD step is attached to <target> which runs:
#   synthxex -i <target> -o <target>.xex
# The output file defaults to "<target>.xex" in the current binary directory.
# ============================================================================

find_program(OPENXECHAIN_SYNTHXEX
    NAMES synthxex
    HINTS "${XECHAIN_SYSROOT}/bin" "${XECHAIN_SYSROOT}/libexec"
    DOC "SynthXEX - the XEX2 builder shipped with the OpenXeChain sysroot")

set(OPENXECHAIN_XEX_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(openxechain_add_xex target)
    set(options "")
    set(oneValueArgs TYPE OUTPUT)
    set(multiValueArgs "")
    cmake_parse_arguments(XEX "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT OPENXECHAIN_SYNTHXEX)
        message(FATAL_ERROR "openxechain_add_xex: synthxex not found. Is the OpenXeChain sysroot built? (looked in \"${XECHAIN_SYSROOT}/bin\")")
    endif()

    if(NOT TARGET "${target}")
        message(FATAL_ERROR "openxechain_add_xex: unknown target \"${target}\"")
    endif()

    if(XEX_OUTPUT)
        set(xex_output "${XEX_OUTPUT}")
    else()
        set(xex_output "${CMAKE_CURRENT_BINARY_DIR}/${target}.xex")
    endif()

    set(type_args "")
    if(XEX_TYPE)
        set(type_args "-t" "${XEX_TYPE}")
    endif()

    add_custom_command(TARGET "${target}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E env python3 "${OPENXECHAIN_XEX_DIR}/PatchDosHeader.py" $<TARGET_FILE:${target}>
        COMMAND "${OPENXECHAIN_SYNTHXEX}" -i "$<TARGET_FILE:${target}>" -o "${xex_output}" ${type_args}
        COMMAND "${CMAKE_COMMAND}" -E echo "Built ${xex_output}"
        VERBATIM)
endfunction()
