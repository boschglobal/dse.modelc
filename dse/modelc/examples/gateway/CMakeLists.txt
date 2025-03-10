# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(VERSION "$ENV{PACKAGE_VERSION}")

project(Gateway
    DESCRIPTION "ModelC - Example Gateway."
    HOMEPAGE_URL "${PROJECT_URL}"
)
set(PROJECT_VERSION ${VERSION})
set(EXE_PATH "examples/gateway")


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


include(FetchContent)

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

# Gateway
# -------
add_executable(gateway
    gateway.c
)
set_target_properties(gateway
    PROPERTIES
        OUTPUT_NAME gateway
        INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib"
)
target_include_directories(gateway
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_compile_definitions(gateway
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_link_libraries(gateway
    PRIVATE
        $<$<BOOL:${WIN32}>:ws2_32>
        ${modelc_shared_lib}
)
install(
    TARGETS
        gateway
    DESTINATION
        ${EXE_PATH}/bin
    COMPONENT
        gateway_model
)
if(WIN32)
install(
    TARGETS
        ${modelc_shared_lib}
    DESTINATION
        ${EXE_PATH}/bin
    COMPONENT
        gateway_model
)
endif()
install(
    FILES
        gateway.yaml
        simulation.yaml
    DESTINATION
        ${EXE_PATH}/data
    COMPONENT
        gateway_model
)
