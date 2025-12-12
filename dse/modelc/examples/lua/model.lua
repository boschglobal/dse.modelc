-- Copyright 2025 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0

SIG_COUNTER = 1
signal_map = {
    [SIG_COUNTER] = "counter",
}
signals = {}


function index_signals()
    for idx, signal in pairs(signal_map) do
        local sv_idx = model.sv["signal"]:find(signal)
        signals[idx] = sv_idx
        model:log_notice("signal indexed: [%d]%s -> sv[\"signal\"].scalar[%d]", idx, signal, sv_idx)
    end
end

function set_initial_values()
    for idx, signal in pairs(signal_map) do
        local annotation = model.sv["signal"]:annotation(signal, "initial_value")
        local value = tonumber(annotation)
        if value ~= nil then
            model.sv["signal"].scalar[signals[idx]] = value
        end
    end
end

function print_signal_values()
    for idx, signal in pairs(signal_map) do
        model:log_notice("signal value: [%d]%s = %f", idx, signal, model.sv["signal"].scalar[signals[idx]])
    end
end


function model_create()
    model:log_notice("model_create()")
    model:log_notice("  step_size: %f", model:step_size())

    -- Debugging info.
    model:log_debug(tostring(model))

    -- Index signals used by this model.
    index_signals()

    -- Signal initial values.
    set_initial_values()
    print_signal_values()

    -- Indicate success.
    return 0
end

function model_step()
    model:log_notice("model_step() @ %f", model:model_time())

    -- Increment the counter signal.
    model.sv["signal"].scalar[signals[SIG_COUNTER]] = model.sv["signal"].scalar[signals[SIG_COUNTER]] + 1
    print_signal_values()

    -- Indicate success.
    return 0
end

function model_destroy()
    model:log_notice("model_destroy()")

    -- Indicate success.
    return 0
end
