# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(ModelC_Mocks)



# Targets
# =======

# Target - mocks
# --------------
add_library(mocks OBJECT
    simmock.c
)
target_compile_definitions(mocks
    PUBLIC
        CMOCKA_TESTING
)
target_include_directories(mocks
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${LUA_SOURCE_DIR}
        ../..
)


# Code examples for documentation.
if(UNIX)
add_subdirectory(examples)
endif()
