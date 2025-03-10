# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(ModelC_Controller)



# Targets
# =======

# Target - controller
# -------------------
add_library(controller OBJECT
    controller.c
    loader.c
    log.c
    mcl_mk1.c
    model_function.c
    model_runtime.c
    modelc.c
    modelc_args.c
    modelc_debug.c
    step.c
    transform.c
)
target_compile_definitions(controller
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_include_directories(controller
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
        ../../..
        $<$<BOOL:${WIN32}>:${DLFCNWIN32_SOURCE_DIR}>
)
target_link_libraries(controller
    PRIVATE
        yaml
        dl
        m
        $<$<BOOL:${UNIX}>:rt>
        $<$<BOOL:${WIN32}>:ws2_32>
        $<$<BOOL:${WIN32}>:iphlpapi>
        $<$<AND:$<BOOL:${WIN32}>,$<STREQUAL:${CMAKE_CXX_COMPILER_ID},"GNU">>:"-static winpthread">
        $<$<AND:$<BOOL:${WIN32}>,$<NOT:$<STREQUAL:${CMAKE_CXX_COMPILER_ID},"GNU">>>:-static pthread>
)
