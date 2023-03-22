# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(VERSION "$ENV{PACKAGE_VERSION}")

project(ModelC
    VERSION ${VERSION}
    DESCRIPTION "Model Step - Model loader and STEPper."
    HOMEPAGE_URL "${PROJECT_URL}"
)

set(CMAKE_ENABLE_EXPORTS ON)



# Targets
# =======

# Model Stepper
# -------------
add_executable(mstep
    mstep.c
)
set_target_properties(mstep
    PROPERTIES
        OUTPUT_NAME mstep
)
target_include_directories(mstep
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        $<$<BOOL:${WIN32}>:${DLFCNWIN32_SOURCE_DIR}>
        ../../../..
)
target_compile_definitions(mstep
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_link_libraries(mstep
    PRIVATE
        $<$<BOOL:${WIN32}>:ws2_32>
    PUBLIC
        -Wl,--whole-archive
        ${modelc_link_lib}
        -Wl,--no-whole-archive
)
install(TARGETS mstep
    COMPONENT
        modelc
)
