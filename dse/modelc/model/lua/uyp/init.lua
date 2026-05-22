-- Copyright 2026 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0

local UYPModel = {}
UYPModel.__index = UYPModel

local function build_index_from_schema(schema)
    local idx = {}

    for space, fields in pairs(schema or {}) do
        idx[space] = {}
        for i, name in ipairs(fields) do
            idx[space][name] = i
        end
    end

    return idx
end

local function build_names_from_schema(schema)
    local names = {}

    for space, fields in pairs(schema or {}) do
        names[space] = {}
        for i, name in ipairs(fields) do
            names[space][i] = name
        end
    end

    return names
end

local function alloc_indexed_array(idx_map)
    local n = 0
    for _, v in pairs(idx_map or {}) do
        if type(v) == "number" and v > n then
            n = v
        end
    end

    local a = {}
    for i = 1, n do
        a[i] = 0
    end
    return a
end

local function apply_named_values(arr, idx_map, named)
    if not named then
        return
    end

    for name, value in pairs(named) do
        local i = idx_map[name]
        if not i then
            error(("unknown index name '%s'"):format(tostring(name)), 2)
        end
        arr[i] = value
    end
end

function UYPModel.new(model, schema, opts)
    opts = opts or {}
    local self = setmetatable({}, UYPModel)

    self.model = model
    self.name = opts.name
    self.schema = schema or {}
    self.idx = build_index_from_schema(self.schema)
    self.names = build_names_from_schema(self.schema)

    self.u = alloc_indexed_array(self.idx.u)
    self.y = alloc_indexed_array(self.idx.y)
    self.p = alloc_indexed_array(self.idx.p)

    apply_named_values(self.u, self.idx.u, opts.u)
    apply_named_values(self.y, self.idx.y, opts.y)
    apply_named_values(self.p, self.idx.p, opts.p)

    self.bindings = {}

    self.signals = {
        group = nil,
        sv = nil,
        scalar = nil,
        u = alloc_indexed_array(self.idx.u),
        y = alloc_indexed_array(self.idx.y),
        p = alloc_indexed_array(self.idx.p),
    }

    self.signal_names = {
        u = alloc_indexed_array(self.idx.u),
        y = alloc_indexed_array(self.idx.y),
        p = alloc_indexed_array(self.idx.p),
    }

    return self
end

function UYPModel:bind_signals(mapping, signal_group)
    signal_group = signal_group or "scalar_vector"

    local sv = self.model.sv[signal_group]
    if not sv then
        self.model:log_error("Signal group '%s' not found", signal_group)
        return -1
    end

    local binding = {
        group = signal_group,
        sv = sv,
        scalar = sv.scalar,
        u = alloc_indexed_array(self.idx.u),
        y = alloc_indexed_array(self.idx.y),
        p = alloc_indexed_array(self.idx.p),
    }

    for space, named in pairs(mapping or {}) do
        local idx_space = self.idx[space]
        if not idx_space then
            error(("unknown space '%s' (expected one of schema spaces)"):format(tostring(space)), 2)
        end
        if type(named) ~= "table" then
            error(("mapping.%s must be a table"):format(tostring(space)), 2)
        end

        binding[space] = alloc_indexed_array(idx_space)

        for key, signal_name in pairs(named) do
            if not idx_space[key] then
                error(("unknown key '%s' for space '%s'"):format(tostring(key), tostring(space)), 2)
            end
            if type(signal_name) ~= "string" then
                error(("mapping.%s.%s must be a signal name string"):format(space, tostring(key)), 2)
            end

            local scalar_idx = sv:find(signal_name)
            if not scalar_idx then
                error(("signal '%s' not found in group '%s'"):format(signal_name, signal_group), 2)
            end

            local item_idx = idx_space[key]
            binding[space][item_idx] = scalar_idx
            self.signal_names[space][item_idx] = signal_name
        end
    end

    table.insert(self.bindings, binding)
    -- Keep self.signals pointing to the primary binding for print functions.
    self.signals = self.bindings[1]

    return 0
end

function UYPModel:read_u()
    if #self.bindings == 0 then
        return -1
    end

    for _, binding in ipairs(self.bindings) do
        local scalar = binding.scalar
        for i = 1, #binding.u do
            local scalar_i = binding.u[i]
            if scalar_i and scalar_i ~= 0 then
                self.u[i] = scalar[scalar_i]
            end
        end
    end

    return 0
end

function UYPModel:set_u(named)
    if not named then
        return 0
    end

    for name, value in pairs(named) do
        local i = self.idx.u and self.idx.u[name]
        if not i then
            error(("unknown u name '%s'"):format(tostring(name)), 2)
        end
        self.u[i] = value
    end

    return 0
end

function UYPModel:set_y(named)
    if not named then
        return 0
    end

    for name, value in pairs(named) do
        local i = self.idx.y and self.idx.y[name]
        if not i then
            error(("unknown y name '%s'"):format(tostring(name)), 2)
        end
        self.y[i] = value
    end

    return 0
end

function UYPModel:write_y()
    if #self.bindings == 0 then
        return -1
    end

    for _, binding in ipairs(self.bindings) do
        local scalar = binding.scalar
        for i = 1, #binding.y do
            local scalar_i = binding.y[i]
            if scalar_i and scalar_i ~= 0 then
                scalar[scalar_i] = self.y[i]
            end
        end
    end

    return 0
end

function UYPModel:write_p()
    if #self.bindings == 0 then
        return -1
    end

    for _, binding in ipairs(self.bindings) do
        local scalar = binding.scalar
        for i = 1, #binding.p do
            local scalar_i = binding.p[i]
            if scalar_i and scalar_i ~= 0 then
                scalar[scalar_i] = self.p[i]
            end
        end
    end

    return 0
end

function UYPModel:init()
end

function UYPModel:update(dt)
    error("UYPModel:update(dt) must be implemented by subclass", 2)
end

local function print_value_group(model, group_title, values, indices, names, scalar, log_notice)
    log_notice("  %s", group_title)
    for i = 1, #values do
        local item_name = names and names[i] or (group_title .. tostring(i))
        local signal_name = "-"
        if model.signal_names[indices] and model.signal_names[indices][i] ~= 0 then
            signal_name = model.signal_names[indices][i]
        end

        local value = values[i]
        local scalar_i = model.signals[indices] and model.signals[indices][i]
        if scalar_i and scalar_i ~= 0 then
            value = scalar[scalar_i]
        end

        log_notice("    [%d] %s = %f (%s)", i, item_name, value, signal_name)
    end
end

function UYPModel:print_values()
    local log_notice = function(fmt, ...)
        self.model:log_notice(fmt, ...)
    end
    log_notice("%s: signals:", self.name or tostring(self):gsub("^table:%s*", ""))

    local scalar = self.signals.scalar
    if not scalar then
        self.model:log_error("signals not bound")
        return -1
    end

    print_value_group(self, "u values:", self.u, "u", self.names.u, scalar, log_notice)
    print_value_group(self, "y values:", self.y, "y", self.names.y, scalar, log_notice)

    return 0
end

function UYPModel:print_parameters()
    local log_notice = function(fmt, ...)
        self.model:log_notice(fmt, ...)
    end
    log_notice("%s: parameters:", self.name or tostring(self):gsub("^table:%s*", ""))

    local scalar = self.signals.scalar
    if not scalar then
        self.model:log_error("signals not bound")
        return -1
    end

    print_value_group(self, "p values:", self.p, "p", self.names.p, scalar, log_notice)

    return 0
end

return UYPModel
