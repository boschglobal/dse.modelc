-- Copyright 2025 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0

-- Signal group name (must match FMU / model configuration)
SIGNAL_GROUP = "signal_channel"

-- Variable indices (matching C fmu_register_var IDs)
local IDX_INPUT  = 1
local IDX_FACTOR = 2
local IDX_OFFSET = 3
local IDX_OUTPUT = 4


-- model_create (C: fmu_create + fmu_init)
function model_create()
    model:log_notice("Linear model_create()")

    local sv = model.sv[SIGNAL_GROUP]
    if not sv then
        model:log_error("Signal group '%s' not found", SIGNAL_GROUP)
        return -1
    end

    -- Optional: log initial values
    model:log_notice(
        "Initial values: input=%f factor=%f offset=%f output=%f",
        sv.scalar[IDX_INPUT],
        sv.scalar[IDX_FACTOR],
        sv.scalar[IDX_OFFSET],
        sv.scalar[IDX_OUTPUT]
    )

    return 0
end


-- model_step (C: fmu_step)
function model_step()
    local sv = model.sv[SIGNAL_GROUP]

    -- Read inputs
    local x = sv.scalar[IDX_INPUT]
    local m = sv.scalar[IDX_FACTOR]
    local c = sv.scalar[IDX_OFFSET]

    -- Linear function:
    -- y = m*x + c
    sv.scalar[IDX_OUTPUT] = (x * m) + c

    -- Log values
    model:log_notice(
        "Step@%.4f: input=%f factor=%f offset=%f output=%f",
        model:model_time(),
        sv.scalar[IDX_INPUT],
        sv.scalar[IDX_FACTOR],
        sv.scalar[IDX_OFFSET],
        sv.scalar[IDX_OUTPUT]
    )

    return 0
end


-- model_destroy (C: fmu_destroy)
function model_destroy()
    model:log_notice("Linear model_destroy()")
    return 0
end
