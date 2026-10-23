# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

project(Binary)

include(FetchContent)
FetchContent_Declare(dse_clib
URL                 $ENV{DSE_CLIB_URL}
HTTP_USERNAME       $ENV{GHE_USER}
HTTP_PASSWORD       $ENV{GHE_TOKEN}
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_INCLUDE_DIR ${dse_clib_SOURCE_DIR})


set(MODEL_PATH "examples/binary")

# Messenger (produces messages).
add_library(messenger SHARED
    messenger.c
)
target_include_directories(messenger
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(messenger
    PRIVATE
        ${modelc_shared_lib}
)
install(TARGETS messenger
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        binary
)

# Observer model (prints received messages).
add_library(observer SHARED
    observer.c
)
target_include_directories(observer
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(observer
    PRIVATE
        ${modelc_shared_lib}
)
install(TARGETS observer
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        binary
)

# Common files.
install(
    FILES
        model.yaml
        simulation.yaml
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        binary
)


# Flowcontrol example for stream transport tests.
set(MODEL_PATH "examples/flowcontrol")
add_library(binary SHARED
    model.c
)
target_include_directories(binary
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(binary
    PRIVATE
        ${modelc_shared_lib}
)
install(TARGETS binary
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    RUNTIME DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        binary
)
install(
    FILES
        model.yaml
        simulation_flowcontrol.yaml
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        binary
)
