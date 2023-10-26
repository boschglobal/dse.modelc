# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(ModelC_Model_API)



# Targets
# =======

# Target - model_api (API Called from Models)
# ------------------
add_library(model_api OBJECT
    gateway.c
    model.c
    ncodec.c
    schema.c
    signal.c
    ${DSE_NCODEC_SOURCE_DIR}/dse/ncodec/codec.c
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
