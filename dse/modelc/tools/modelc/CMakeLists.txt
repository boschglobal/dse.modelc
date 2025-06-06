# Copyright 2023 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(VERSION "$ENV{PACKAGE_VERSION}")

project(ModelC
    DESCRIPTION "ModelC - Runtime for Models."
    HOMEPAGE_URL "${PROJECT_URL}"
)
set(PROJECT_VERSION ${VERSION})
set(CMAKE_ENABLE_EXPORTS ON)



# Targets
# =======

# ModelC CLI
# ----------
add_executable(modelc_cli
    modelc.c
)
set_target_properties(modelc_cli
    PROPERTIES
        OUTPUT_NAME modelc
        INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib"
)
target_include_directories(modelc_cli
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_compile_definitions(modelc_cli
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_link_libraries(modelc_cli
    PRIVATE
        $<$<BOOL:${WIN32}>:ws2_32>
        ${modelc_shared_lib}
)
install(TARGETS modelc_cli
    COMPONENT
        modelc
)
