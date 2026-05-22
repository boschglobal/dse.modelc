-- Copyright 2026 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0

local UYPModel = require("uyp")

local Controller = setmetatable({}, { __index = UYPModel })
Controller.__index = Controller

local CONTROLLER_SCHEMA = {
    u = { "TARGET_SPEED", "MEASURED_SPEED" },
    y = { "THROTTLE", "SPEED_ERROR", "INTEGRAL_ERROR", "DERIVATIVE_ERROR" },
    p = { "KP", "KI", "KD" },
}

function Controller.new(model, opts)
    local self = UYPModel.new(model, CONTROLLER_SCHEMA, opts)
    self._integral = 0.0
    self._prev_error = 0.0
    return setmetatable(self, Controller)
end

function Controller:init()
    local y, idx = self.y, self.idx
    y[idx.y.THROTTLE] = 0.0
    y[idx.y.SPEED_ERROR] = 0.0
    y[idx.y.INTEGRAL_ERROR] = 0.0
    y[idx.y.DERIVATIVE_ERROR] = 0.0
    self._integral = 0.0
    self._prev_error = 0.0
    return 0
end

function Controller:update(dt)
    local u, y, p, idx = self.u, self.y, self.p, self.idx

    local target_speed = u[idx.u.TARGET_SPEED]
    local measured_speed = u[idx.u.MEASURED_SPEED]

    local error = target_speed - measured_speed
    self._integral = self._integral + error * dt

    local derivative = 0.0
    if dt > 0.0 then
        derivative = (error - self._prev_error) / dt
    end

    local throttle =
        p[idx.p.KP] * error +
        p[idx.p.KI] * self._integral +
        p[idx.p.KD] * derivative

    y[idx.y.THROTTLE] = throttle
    y[idx.y.SPEED_ERROR] = error
    y[idx.y.INTEGRAL_ERROR] = self._integral
    y[idx.y.DERIVATIVE_ERROR] = derivative

    self._prev_error = error
    return 0
end

return Controller