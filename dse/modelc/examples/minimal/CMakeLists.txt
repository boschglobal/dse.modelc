# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(Minimal)
set(MODEL_PATH "examples/minimal")

add_library(minimal SHARED
    model.c
)
target_include_directories(minimal
    PRIVATE
        ../../../..
)
target_link_libraries(minimal
    PRIVATE
        ${modelc_shared_lib}
)
install(TARGETS minimal
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        minimal
)
install(
    FILES
        model.yaml
        simulation.yaml
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        minimal
)
