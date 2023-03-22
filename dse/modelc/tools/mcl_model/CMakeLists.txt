# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(VERSION "$ENV{PACKAGE_VERSION}")

project(MCL_Model
    VERSION ${VERSION}
    DESCRIPTION "MCL Model - Model Container for MCL instances."
    HOMEPAGE_URL "${PROJECT_URL}"
)

include(FetchContent)

set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_ENABLE_EXPORTS ON)


# External Project - DSE C Lib
# ----------------------------
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{DSE_CLIB_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_CLIB_URL_TOKEN}
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse)
set(DSE_CLIB_SOURCE_FILES )
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/..")



# Targets
# =======

# MCL Model
# ---------
add_library(mcl_model SHARED
    model.c
)
target_include_directories(mcl_model
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_compile_definitions(mcl_model
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(mcl_model
    PRIVATE
        $<$<BOOL:${WIN32}>:${modelc_link_lib}>
)
install(TARGETS mcl_model
    COMPONENT
        modelc
)
install(
    FILES
        mcl_model.yaml
    DESTINATION
        data
    COMPONENT
        modelc
)
