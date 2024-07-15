# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(ModelC_Model_API)




# Targets
# =======

FetchContent_Declare(automotive-bus-schema
    URL https://github.com/boschglobal/automotive-bus-schema/releases/download/v1.0.3/automotive-bus-schema.tar.gz
)
FetchContent_MakeAvailable(automotive-bus-schema)
set(SCHEMAS_SOURCE_DIR ${automotive-bus-schema_SOURCE_DIR}/flatbuffers/c)
set(FLATCC_SOURCE_DIR ${SCHEMAS_SOURCE_DIR}/automotive_bus_schema/flatcc/src)
set(FLATCC_INCLUDE_DIR ${SCHEMAS_SOURCE_DIR}/automotive_bus_schema/flatcc/include)


# Target - dse_ncodec
# -------------------
add_library(dse_ncodec OBJECT
    ncodec.c
    ${DSE_NCODEC_SOURCE_DIR}/codec.c
    ${DSE_NCODEC_SOURCE_DIR}/libs/automotive-bus/codec.c
    ${DSE_NCODEC_SOURCE_DIR}/libs/automotive-bus/frame_can_fbs.c
)
target_include_directories(dse_ncodec
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}/dse/ncodec/libs
        ${FLATCC_INCLUDE_DIR}
        ${SCHEMAS_SOURCE_DIR}
        ../../..
)
target_link_libraries(dse_ncodec
    PRIVATE
        flatcc_runtime
)


# Target - model_api (API Called from Models)
# ------------------
add_library(model_api OBJECT
    gateway.c
    mcl.c
    model.c
    schema.c
    signal.c
    trace.c
)
target_include_directories(model_api
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
        ../../..
)
target_link_libraries(model_api
    PRIVATE
        dse_ncodec
        yaml
        m
        $<$<BOOL:${WIN32}>:ws2_32>
        $<$<BOOL:${WIN32}>:iphlpapi>
        $<$<AND:$<BOOL:${WIN32}>,$<STREQUAL:${CMAKE_CXX_COMPILER_ID},"GNU">>:"-static winpthread">
)
