# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(VERSION "$ENV{PACKAGE_VERSION}")

project(SimBus
    DESCRIPTION "SimBus - Standalone SimBus."
    HOMEPAGE_URL "${PROJECT_URL}"
)
set(PROJECT_VERSION ${VERSION})


# Targets
# =======

# SimBus
# ------
add_executable(simbus
    simbus.c
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
        clib_api
        controller
        ab-codec
        model_api
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
