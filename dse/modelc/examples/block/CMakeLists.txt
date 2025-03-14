# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0


cmake_minimum_required(VERSION 3.21)

project(Block)
set(MODEL_PATH "examples/block")

include(FetchContent)
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{GHE_USER}
    HTTP_PASSWORD       $ENV{GHE_TOKEN}
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_INCLUDE_DIR ${dse_clib_SOURCE_DIR})

add_library(block SHARED
    model.c
)
target_include_directories(block
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(block
    PRIVATE
        ${modelc_shared_lib}
)
install(TARGETS block
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        block
)
install(
    FILES
        model.yaml
        simulation.yaml
        signal_group.yaml
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        block
)
