# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

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
        INSTALL_RPATH "$ORIGIN/../lib"
)
target_include_directories(importer
    PRIVATE
        ../../../..
        ${DSE_NCODEC_INCLUDE_DIR}
)
target_compile_definitions(importer
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${VERSION}"
)
target_link_libraries(importer
    PRIVATE
        dse_external_dl
        ${modelc_shared_lib}
        m
        $<$<BOOL:${UNIX}>:mcheck>
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(importer PRIVATE "LINKER:--no-as-needed")
endif()
install(TARGETS importer RUNTIME
    DESTINATION
        ${EX_PATH}/bin
)


# Simulation Models
# ------------------
add_library(csv_rm SHARED
    sim/csv_input.c
    ${DSE_CLIB_SOURCE_DIR}/clib/csv/csv.c
)
add_library(linear_rm SHARED
sim/linear_model.c
)
add_library(observer_rm SHARED
sim/observer_model.c
)
set_target_properties(csv_rm PROPERTIES OUTPUT_NAME "csv")
set_target_properties(linear_rm PROPERTIES OUTPUT_NAME "linear")
set_target_properties(observer_rm PROPERTIES OUTPUT_NAME "observer")
foreach(model csv_rm linear_rm observer_rm)
    target_include_directories(${model}
        PRIVATE
            ${DSE_CLIB_INCLUDE_DIR}
            ${DSE_NCODEC_INCLUDE_DIR}
            ../../../..
    )
    target_link_libraries(${model}
        PRIVATE
            ${modelc_shared_lib}
    )
    set_target_properties(${model}
        PROPERTIES
            INSTALL_RPATH "$ORIGIN/../../../../lib"
    )
endforeach()
install(
    TARGETS
        csv_rm
    LIBRARY DESTINATION ${SIM_PATH}/model/input/lib
    RUNTIME DESTINATION ${SIM_PATH}/model/input/lib
)
install(
    FILES
        sim/model/input/data/model.yaml
        sim/model/input/data/signalgroup.yaml
        sim/model/input/data/input.csv
    DESTINATION ${SIM_PATH}/model/input/data
)
install(
    TARGETS linear_rm
    LIBRARY DESTINATION ${SIM_PATH}/model/linear/lib
    RUNTIME DESTINATION ${SIM_PATH}/model/linear/lib
)
install(
    FILES
        sim/model/linear/data/model.yaml
        sim/model/linear/data/signalgroup.yaml
    DESTINATION ${SIM_PATH}/model/linear/data
)
install(
    TARGETS observer_rm
    LIBRARY DESTINATION ${SIM_PATH}/model/observer/lib
    RUNTIME DESTINATION ${SIM_PATH}/model/observer/lib
)
install(
    FILES
        sim/model/observer/data/model.yaml
        sim/model/observer/data/signalgroup.yaml
    DESTINATION ${SIM_PATH}/model/observer/data
)
install(
    FILES sim/data/simulation.yaml
    DESTINATION ${SIM_PATH}/data
)


# Runtime (implements importer interface, runs the simulation)
# -------
add_library(runtime SHARED
    runtime.c
)
set_target_properties(runtime PROPERTIES
    INSTALL_RPATH "$ORIGIN"
)
target_include_directories(runtime
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(runtime
    PRIVATE
        ${modelc_shared_lib}
)
install(
    TARGETS
        runtime
        ${modelc_shared_lib}
    LIBRARY DESTINATION ${EX_PATH}/lib
    RUNTIME DESTINATION ${EX_PATH}/lib
)
