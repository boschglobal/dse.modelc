# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

project(PduNet)


include(FetchContent)
FetchContent_Declare(dse_clib
URL                 $ENV{DSE_CLIB_URL}
HTTP_USERNAME       $ENV{GHE_USER}
HTTP_PASSWORD       $ENV{GHE_TOKEN}
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_INCLUDE_DIR ${dse_clib_SOURCE_DIR})

# Flexray example
set(MODEL_NAME "frnet")
set(MODEL_PATH "examples/pdunet/${MODEL_NAME}")
add_library(${MODEL_NAME} SHARED
    ${MODEL_NAME}/model.c
)
target_include_directories(${MODEL_NAME}
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(${MODEL_NAME}
    PRIVATE
        ${modelc_shared_lib}
)
install(TARGETS ${MODEL_NAME}
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        ${MODEL_NAME}
)
install(
    FILES
        ${MODEL_NAME}/model.yaml
        ${MODEL_NAME}/network.yaml
        ${MODEL_NAME}/simulation.yaml
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        ${MODEL_NAME}
)
