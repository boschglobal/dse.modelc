-- Copyright 2026 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0

package.path = package.path .. ";./model/cruise/lua/?.lua;./model/cruise/lua/?/init.lua"

local Controller = require("controller")
local Plant = require("plant")
local Driver = require("driver")

local controller
local plant
local driver

function model_create()
    -- PID controller.
    controller = Controller.new(model, {
        name = "PID Controller",
        p = {
            KP = 0.8,
            KI = 0.2,
            KD = 0.05,
        },
    })
    controller:bind_signals({
        u = {
            TARGET_SPEED = "target_speed",
            MEASURED_SPEED = "vehicle_speed",
        },
        y = {
            THROTTLE = "throttle_cmd",
            SPEED_ERROR = "speed_error",
            INTEGRAL_ERROR = "speed_error_i",
            DERIVATIVE_ERROR = "speed_error_d",
        },
        p = {
            KP = "controller_kp",
            KI = "controller_ki",
            KD = "controller_kd",
        },
    }, "scalar_vector")
    controller:init()
    controller:write_p()
    controller:write_y()

    -- Vehicle plant.
    plant = Plant.new(model, {
        name = "Vehicle Plant",
        p = {
            TAU = 2.5,
            GAIN = 1.0,
            INIT_SPEED = 0.0,
        },
    })
    plant:bind_signals({
        u = {
            THROTTLE = "throttle_cmd",
        },
        y = {
            SPEED = "vehicle_speed",
        },
        p = {
            TAU = "plant_tau",
            GAIN = "plant_gain",
            INIT_SPEED = "plant_init_speed",
        },
    }, "scalar_vector")
    plant:init()
    plant:write_p()
    plant:write_y()

    -- Driver target speed command.
    driver = Driver.new(model, {
        name = "Driver",
        p = {
            TARGET_SPEED = 30.0,
        },
    })
    driver:bind_signals({
        y = {
            TARGET_SPEED = "target_speed",
        },
        p = {
            TARGET_SPEED = "driver_target_speed",
        },
    }, "scalar_vector")
    driver:init()
    driver:write_p()
    driver:write_y()

    controller:print_parameters()
    plant:print_parameters()

    controller:print_values()
    plant:print_values()

    return 0
end

function model_step()
    local dt = model:step_size()
    model:log_notice("model_step() @ %f", model:model_time())

    driver:update(dt)
    driver:write_y()

    controller:read_u()
    controller:update(dt)
    controller:write_y()

    plant:read_u()
    plant:update(dt)
    plant:write_y()

    driver:print_values()
    controller:print_values()
    plant:print_values()

    return 0
end

function model_destroy()
    model:log_info("Model destroyed at time %.4f", model:model_time())
    return 0
end