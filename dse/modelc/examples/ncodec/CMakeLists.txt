# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(NCodec)
set(MODEL_PATH "examples/ncodec")

include(FetchContent)
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{GHE_USER}
    HTTP_PASSWORD       $ENV{GHE_TOKEN}
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_INCLUDE_DIR ${dse_clib_SOURCE_DIR})


add_library(ncodec_can SHARED
    model_can.c
)
target_include_directories(ncodec_can
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(ncodec_can
    PRIVATE
        $<$<BOOL:${WIN32}>:${modelc_link_lib}>
)
install(TARGETS ncodec_can
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        ncodec
)

add_library(ncodec_pdu SHARED
    model_pdu.c
)
target_include_directories(ncodec_pdu
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(ncodec_pdu
    PRIVATE
        $<$<BOOL:${WIN32}>:${modelc_link_lib}>
)
install(TARGETS ncodec_pdu
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        ncodec
)

install(
    FILES
        model.yaml
        simulation.yaml
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        ncodec
)
