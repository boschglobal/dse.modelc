-- Copyright 2026 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0

SV = "scalar_vector"
SIG_ALIVE    = 1
SIG_ALIVE_RX = 2
SIG_FOO      = 3
SIG_BAR      = 4
signal_map = {
    [SIG_ALIVE]    = "Alive",
    [SIG_ALIVE_RX] = "AliveRx",
    [SIG_FOO]      = "FOO",
    [SIG_BAR]      = "BAR",
}
signals = {}


function index_signals()
    for idx, signal in pairs(signal_map) do
        local sv_idx = model.sv[SV]:find(signal)
        signals[idx] = sv_idx
        model:log_notice("  signal: [%d]%s -> sv[\"%s\"].scalar[%d]",
            idx, signal, SV, sv_idx)
    end
end

function model_create()
    model:log_notice("model_create()")
    model:log_debug(tostring(model))

    -- Index signals used by this model.
    index_signals()

    -- Indicate success.
    return 0
end

function model_step()
    -- Update signals.
    local sample = { 42, 24, 22, 44 }
    local v = model.sv[SV].scalar[signals[SIG_ALIVE_RX]]
    model.sv[SV].scalar[signals[SIG_FOO]] = sample[(v % 4) + 1]

    model:log_notice("step[%f]: Alive=%f, AliveRx=%f, FOO=%f, BAR=%f",
        model:model_time(),
        model.sv[SV].scalar[signals[SIG_ALIVE]],
        model.sv[SV].scalar[signals[SIG_ALIVE_RX]],
        model.sv[SV].scalar[signals[SIG_FOO]],
        model.sv[SV].scalar[signals[SIG_BAR]]
    )

    -- Indicate success.
    return 0
end
