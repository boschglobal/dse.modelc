# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(Gdb)
set(MODEL_PATH "examples/gdb")

add_library(gdb SHARED
    model.c
)
target_include_directories(gdb
    PRIVATE
        ../../../..
)
target_link_libraries(gdb
    PRIVATE
        ${modelc_shared_lib}
)
install(TARGETS gdb
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        gdb
)
install(
    FILES
        model.yaml
        simulation.yaml
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        gdb
)
