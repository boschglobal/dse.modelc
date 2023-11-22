# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(StackedModel
    DESCRIPTION "ModelC - Example Stacked Model."
    HOMEPAGE_URL "${PROJECT_URL}"
)
set(PROJECT_VERSION ${VERSION})

include(FetchContent)

set(MODEL_PATH "examples/stacked")
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS_DEBUG "-g -ggdb")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O3")
list(APPEND C_CXX_WARNING_FLAGS
    -Wall
    -W
    -Wwrite-strings
    -Wno-missing-field-initializers
    -Wno-misleading-indentation
)
add_compile_options(${C_CXX_WARNING_FLAGS})


# External Project - DSE C Lib
# ----------------------------
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{GHE_USER}
    HTTP_PASSWORD       $ENV{GHE_TOKEN}
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse)
set(DSE_CLIB_SOURCE_FILES )
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/..")



# Targets
# =======

# Target - stacked_one
# ----------------------
add_library(stacked_one SHARED
    stacked_model.c
)
target_include_directories(stacked_one
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(stacked_one
    PRIVATE
        $<$<BOOL:${WIN32}>:${modelc_link_lib}>
)
install(TARGETS stacked_one
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        stacked_one
)


# Target - stacked_two
# ----------------------
add_library(stacked_two SHARED
    stacked_model.c
)
target_include_directories(stacked_two
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(stacked_two
    PRIVATE
        $<$<BOOL:${WIN32}>:${modelc_link_lib}>
)
install(TARGETS stacked_two
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
    COMPONENT
        stacked_two
)



install(
    FILES
        model.yaml
        stack.yaml
        signal_group.yaml
    DESTINATION
        ${MODEL_PATH}/data
)
