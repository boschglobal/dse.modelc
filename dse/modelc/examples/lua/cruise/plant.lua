-- Copyright 2026 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0

local UYPModel = require("uyp")

local Plant = setmetatable({}, { __index = UYPModel })
Plant.__index = Plant

local PLANT_SCHEMA = {
    u = { "THROTTLE" },
    y = { "SPEED" },
    p = { "TAU", "GAIN", "INIT_SPEED" },
}

function Plant.new(model, opts)
    local self = UYPModel.new(model, PLANT_SCHEMA, opts)
    return setmetatable(self, Plant)
end

function Plant:init()
    local p, y, idx = self.p, self.y, self.idx
    y[idx.y.SPEED] = p[idx.p.INIT_SPEED]
    return 0
end

function Plant:update(dt)
    local u, y, p, idx = self.u, self.y, self.p, self.idx

    local throttle = u[idx.u.THROTTLE]
    local speed = y[idx.y.SPEED]
    local tau = p[idx.p.TAU]
    local gain = p[idx.p.GAIN]

    if tau <= 0.0 then
        error("Plant parameter TAU must be > 0", 2)
    end

    local speed_dot = (-speed + gain * throttle) / tau
    y[idx.y.SPEED] = speed + dt * speed_dot

    return 0
end

return Plant