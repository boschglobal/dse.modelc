# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(VERSION "$ENV{PACKAGE_VERSION}")

project(SimBus
    VERSION ${VERSION}
    DESCRIPTION "SimBus - Standalone SimBus."
    HOMEPAGE_URL "${PROJECT_URL}"
)



# Targets
# =======

# SimBus
# ------
add_executable(simbus
    simbus.c
    ${DSE_CLIB_SOURCE_FILES}
)
target_include_directories(simbus
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
        ../../../..
)
target_compile_definitions(simbus
    PRIVATE
        MODELC_VERSION="${VERSION}"
)
target_link_libraries(simbus
    PRIVATE
        adapter
        controller
        model_api
        $<$<BOOL:${WIN32}>:ws2_32>
        $<$<BOOL:${WIN32}>:iphlpapi>
        $<$<AND:$<BOOL:${WIN32}>,$<STREQUAL:${CMAKE_CXX_COMPILER_ID},"GNU">>:"-static winpthread">
)
install(TARGETS simbus
    COMPONENT
        modelc
)
install(
    FILES
        simbus.yaml
    DESTINATION
        data/yaml
    COMPONENT
        modelc
)
