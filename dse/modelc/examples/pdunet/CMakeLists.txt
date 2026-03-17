# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

project(PduNet)


include(FetchContent)
FetchContent_Declare(dse_clib
URL                 $ENV{DSE_CLIB_URL}
HTTP_USERNAME       $ENV{GHE_USER}
HTTP_PASSWORD       $ENV{GHE_TOKEN}
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_INCLUDE_DIR ${dse_clib_SOURCE_DIR})


#   dse_add_pdunet_c_model(
#     NAME <target_name>
#     PATH <install_path_relative_to_prefix>
#     SOURCES <src1> [<src2> ...]
#     DATA_FILES <file1> [<file2> ...]
#   )
function(dse_add_pdunet_c_model)
    set(options)
    set(oneValueArgs NAME PATH)
    set(multiValueArgs SOURCES DATA_FILES)
    cmake_parse_arguments(M "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_library(${M_NAME} SHARED ${M_SOURCES})

    target_include_directories(${M_NAME}
        PRIVATE
            ${DSE_CLIB_INCLUDE_DIR}
            ${DSE_NCODEC_INCLUDE_DIR}
            ../../../..
    )

    target_link_libraries(${M_NAME}
        PRIVATE
            ${modelc_shared_lib}
    )

    install(TARGETS ${M_NAME}
        LIBRARY DESTINATION
            ${M_PATH}/lib
        RUNTIME DESTINATION
            ${M_PATH}/lib
        COMPONENT
            ${M_NAME}
    )

    if(M_DATA_FILES)
        install(
            FILES
                ${M_DATA_FILES}
            DESTINATION
                ${M_PATH}/data
            COMPONENT
                ${M_NAME}
        )
    endif()
endfunction()

#   dse_add_pdunet_lua_model(
#     NAME <model_name>
#     PATH <install_path_relative_to_prefix>
#     INST <instance_name>
#     SIMULATION_YAML <file>
#     NETWORK_YAML <file>
#     MODEL_LUA <file>
#   )
function(dse_add_pdunet_lua_model)
    set(options)
    set(oneValueArgs NAME PATH INST SIMULATION_YAML NETWORK_YAML MODEL_LUA)
    set(multiValueArgs)
    cmake_parse_arguments(M "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    install(
        FILES
            ${M_SIMULATION_YAML}
        DESTINATION
            ${M_PATH}/data
        COMPONENT
            ${M_NAME}
    )
    install(
        FILES
            ${M_NETWORK_YAML}
        DESTINATION
            ${M_PATH}/model/${M_INST}/data
        COMPONENT
            ${M_NAME}
    )
    install(
        FILES
            ${M_MODEL_LUA}
        DESTINATION
            ${M_PATH}/model/${M_INST}
        COMPONENT
            ${M_NAME}
    )
endfunction()




# Flexray example.
set(MODEL_NAME "frnet")
set(MODEL_PATH "examples/pdunet/${MODEL_NAME}")
dse_add_pdunet_c_model(
    NAME ${MODEL_NAME}
    PATH ${MODEL_PATH}
    SOURCES
        ${MODEL_NAME}/model.c
    DATA_FILES
        ${MODEL_NAME}/model.yaml
        ${MODEL_NAME}/network.yaml
        ${MODEL_NAME}/simulation.yaml
)

# Lua example.
set(MODEL_NAME "luafrnet")
set(MODEL_INST "pdunet_inst")
set(MODEL_PATH "examples/pdunet/${MODEL_NAME}")
dse_add_pdunet_lua_model(
    NAME ${MODEL_NAME}
    PATH ${MODEL_PATH}
    INST ${MODEL_INST}
    SIMULATION_YAML ${MODEL_NAME}/simulation.yaml
    NETWORK_YAML    ${MODEL_NAME}/network.yaml
    MODEL_LUA       ${MODEL_NAME}/model.lua
)

# Container PDU example.
set(MODEL_NAME "container")
set(MODEL_PATH "examples/pdunet/${MODEL_NAME}")
dse_add_pdunet_c_model(
    NAME ${MODEL_NAME}
    PATH ${MODEL_PATH}
    SOURCES
        ${MODEL_NAME}/model.c
    DATA_FILES
        ${MODEL_NAME}/model.yaml
        ${MODEL_NAME}/network.yaml
        ${MODEL_NAME}/simulation.yaml
)

# Secure PDU example.
set(MODEL_NAME "sec")
set(MODEL_PATH "examples/pdunet/${MODEL_NAME}")
dse_add_pdunet_c_model(
    NAME ${MODEL_NAME}
    PATH ${MODEL_PATH}
    SOURCES
        ${MODEL_NAME}/model.c
    DATA_FILES
        ${MODEL_NAME}/model.yaml
        ${MODEL_NAME}/network.yaml
        ${MODEL_NAME}/simulation.yaml
)
