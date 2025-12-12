-- Copyright 2025 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0


function model_create()
    model:log_notice("model_create()")

    -- Logging API.
    model:log_error("error msg")
    model:log_notice("notice msg")
    model:log_info("info msg")
    model:log_debug("debug msg")

    -- Model Methods.
    print("model:version() = " .. model:version())
    print("model:step_size() = " .. model:step_size())
    print("model:model_time() = " .. model:model_time())

    -- SV Methods.
    print("model.sv[]:find() = " .. model.sv["signal"]:find("counter"))
    print("model.sv[]:annotation() = " .. model.sv["signal"]:annotation("counter", "initial_value"))
    print("model.sv[]:group_annotation() = " .. model.sv["signal"]:group_annotation("vector_name"))

    -- SV Access.
    local sv_idx = model.sv["signal"]:find("counter")
    print("len(model.sv) = " .. #model.sv)
    print("len(model.sv[].signal) = " .. #model.sv["signal"].signal)
    print("len(model.sv[].scalar) = " .. #model.sv["signal"].scalar)
    print("model.sv[].signal[] = " .. model.sv["signal"].signal[sv_idx])
    print("model.sv[].signal[] = " .. model.sv["signal"].scalar[sv_idx])

    -- Set initial values.
    local annotation = model.sv["signal"]:annotation("counter", "initial_value")
    model.sv["signal"].scalar[sv_idx] = tonumber(annotation)

    -- Indicate success.
    return 0
end

function model_step()
    model:log_notice("model_step() @ %f", model:model_time())

    -- Increment the counter signal.
    local sv_idx = model.sv["signal"]:find("counter")
    model.sv["signal"].scalar[sv_idx] = model.sv["signal"].scalar[sv_idx] + 1
    print("model.sv[].signal[] = " .. model.sv["signal"].scalar[sv_idx])

    -- Indicate success.
    return 0
end

function model_destroy()
    model:log_notice("model_destroy()")

    -- Indicate success.
    return 0
end
