# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(Simer)
set(MODEL_PATH "examples/simer")

include(FetchContent)
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{GHE_USER}
    HTTP_PASSWORD       $ENV{GHE_TOKEN}
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_INCLUDE_DIR ${dse_clib_SOURCE_DIR})

add_library(counter SHARED
    model.c
)
target_include_directories(counter
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(counter
    PRIVATE
        ${modelc_shared_lib}
)
install(TARGETS counter
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        counter
)
install(
    FILES
        model.yaml
        signalgroup.yaml
        simulation.yaml
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        counter
)
