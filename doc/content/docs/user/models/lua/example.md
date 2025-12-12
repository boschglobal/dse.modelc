SIG_COUNTER = 1

function model_create()
    model:log_notice("model_create()")

    -- Indicate success.
    return 0
end

function model_step()
    model:log_notice("model_step() @ %f", model:model_time())

    -- Increment the counter signal.
    local counter = model.sv["signal"].scalar[SIG_COUNTER]
    counter = counter + 1
    model.sv["signal"].scalar[SIG_COUNTER] = counter

    -- Indicate success.
    return 0
end

function model_destroy()
    model:log_notice("model_destroy()")

    -- Indicate success.
    return 0
end