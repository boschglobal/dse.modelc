# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(ModelC_Model_API)

# Target - model_api (API Called from Models)
# ------------------
file(GLOB PDUNET_SOURCES CONFIGURE_DEPENDS "pdunet/*.c")
add_library(model_api OBJECT
    $<$<BOOL:${UNIX}>:checks.c>
    gateway.c
    lua.c
    luamodel.c
    mcl.c
    model.c
    ncodec.c
    pdunet.c
    ${PDUNET_SOURCES}
    schema.c
    signal.c
    trace.c
)
target_include_directories(model_api
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${LUA_SOURCE_DIR}
        ${YAML_SOURCE_DIR}/include
        ../../..
)
target_compile_definitions(model_api
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_link_libraries(model_api
    PUBLIC
        ab-codec
    PRIVATE
        yaml
        lua
        m
        $<$<BOOL:${WIN32}>:ws2_32>
        $<$<BOOL:${WIN32}>:iphlpapi>
        $<$<AND:$<BOOL:${WIN32}>,$<STREQUAL:${CMAKE_CXX_COMPILER_ID},"GNU">>:"-static winpthread">
)
