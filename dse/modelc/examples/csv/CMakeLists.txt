# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(Csv)
set(MODEL_PATH "examples/csv")

include(FetchContent)
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{GHE_USER}
    HTTP_PASSWORD       $ENV{GHE_TOKEN}
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_INCLUDE_DIR ${dse_clib_SOURCE_DIR})

add_library(csv SHARED
    model.c
)
target_include_directories(csv
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(csv
    PRIVATE
        $<$<BOOL:${WIN32}>:${modelc_link_lib}>
)
install(TARGETS csv
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        csv
)
install(
    FILES
        model.yaml
        simulation.yaml
        valueset.csv
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        csv
)
