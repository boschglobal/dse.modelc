# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

project(lua)


set(MODEL_NAME "model")
set(MODEL_PATH "examples/lua/${MODEL_NAME}")
install(
    FILES
        simulation.yaml
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        ${MODEL_NAME}
)
install(
    FILES
    model.lua
    DESTINATION
        ${MODEL_PATH}/model/lua_inst
    COMPONENT
        ${MODEL_NAME}
)
install(
    FILES
        signalgroup.yaml
    DESTINATION
        ${MODEL_PATH}/model/lua_inst/data
    COMPONENT
        ${MODEL_NAME}
)



set(MODEL_NAME "modellib_api")
set(MODEL_PATH "examples/lua/${MODEL_NAME}")
install(
    FILES
        simulation.yaml
    DESTINATION
        ${MODEL_PATH}/data
    COMPONENT
        ${MODEL_NAME}
)
install(
    FILES
        modellib_api.lua
    DESTINATION
        ${MODEL_PATH}/model/lua_inst
    RENAME
        model.lua
    COMPONENT
        ${MODEL_NAME}
)
install(
    FILES
        signalgroup.yaml
    DESTINATION
        ${MODEL_PATH}/model/lua_inst/data
    COMPONENT
        ${MODEL_NAME}
)



set(MODEL_NAME "cruise")
set(MODEL_PATH "examples/lua/${MODEL_NAME}")
install(
    FILES
        cruise/controller.lua
        cruise/cruise.lua
        cruise/cruise.yaml
        cruise/plant.lua
        cruise/simulation.dse
    DESTINATION
        ${MODEL_PATH}
    COMPONENT
        ${MODEL_NAME}
)
