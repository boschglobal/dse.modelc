# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)
set(VERSION "$ENV{PACKAGE_VERSION}")

project(ModelC
    DESCRIPTION "Runtime example."
    HOMEPAGE_URL "${PROJECT_URL}"
)
set(PROJECT_VERSION ${VERSION})
set(EX_PATH "examples/runtime")
set(SIM_PATH "${EX_PATH}/sim")

set(CMAKE_ENABLE_EXPORTS ON)


# Targets
# =======

# Importer
# --------
add_executable(importer
    importer.c
)
set_target_properties(importer
    PROPERTIES
        OUTPUT_NAME importer
)
target_include_directories(importer
    PRIVATE
        $<$<BOOL:${WIN32}>:${DLFCNWIN32_SOURCE_DIR}>
        ../../../..
)
target_compile_definitions(importer
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_link_libraries(importer
    PRIVATE
        dl
        m
        $<$<BOOL:${UNIX}>:mcheck>
)
install(TARGETS importer RUNTIME
    DESTINATION
        ${EX_PATH}/bin
)


# Simulation (target Model)
# ----------
add_library(target SHARED
    sim/model.c
)
target_include_directories(target
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(target
    PRIVATE
        $<$<BOOL:${WIN32}>:${modelc_link_lib}>
)
install(TARGETS target
    LIBRARY DESTINATION
        ${SIM_PATH}/lib
    RUNTIME DESTINATION
        ${SIM_PATH}/lib
)
install(
    FILES
        sim/model.yaml
        sim/simulation.yaml
    DESTINATION
        ${SIM_PATH}/data
)


# Runtime Model (implements importer interface, runs the simulation)
# -------------
add_library(runtime SHARED
    runtime.c
)
target_include_directories(runtime
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(runtime
    PRIVATE
        -Wl,--whole-archive
        ${model_runtime_link_lib}
        -Wl,--no-whole-archive
)
install(TARGETS runtime
    LIBRARY DESTINATION
        ${EX_PATH}/lib
    RUNTIME DESTINATION
        ${EX_PATH}/lib
)
