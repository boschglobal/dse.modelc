# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

project(Transform)
set(MODEL_PATH "examples/transform")

add_library(ponger SHARED
    ponger.c
)
target_include_directories(ponger
    PRIVATE
        ../../../..
)
target_link_libraries(ponger
    PRIVATE
        ${modelc_shared_lib}
)
install(TARGETS ponger
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        transform
)
add_library(pingit SHARED
    pingit.c
)
target_include_directories(pingit
    PRIVATE
        ../../../..
)
target_link_libraries(pingit
    PRIVATE
        ${modelc_shared_lib}
)
install(TARGETS pingit
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        transform
)
install(
    FILES
        model.yaml
        simulation.yaml
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        transform
)
