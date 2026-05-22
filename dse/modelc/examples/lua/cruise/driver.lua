-- Copyright 2026 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0

local UYPModel = require("uyp")

local Driver = setmetatable({}, { __index = UYPModel })
Driver.__index = Driver

local DRIVER_SCHEMA = {
    u = {},
    y = { "TARGET_SPEED" },
    p = { "TARGET_SPEED" },
}

function Driver.new(model, opts)
    local self = UYPModel.new(model, DRIVER_SCHEMA, opts)
    return setmetatable(self, Driver)
end

function Driver:init()
    local y, p, idx = self.y, self.p, self.idx
    y[idx.y.TARGET_SPEED] = p[idx.p.TARGET_SPEED]
    return 0
end

function Driver:update(_dt)
    local y, p, idx = self.y, self.p, self.idx
    y[idx.y.TARGET_SPEED] = p[idx.p.TARGET_SPEED]
    return 0
end

return Driver