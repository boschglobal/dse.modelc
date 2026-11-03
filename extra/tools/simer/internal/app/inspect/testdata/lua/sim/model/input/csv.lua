-- Copyright 2025 Robert Bosch GmbH
--
-- SPDX-License-Identifier: Apache-2.0

CSV_FILE_ENVAR = "CSV_FILE"
CSV_DELIM_PATTERN = "([^;]+)"
SIGNAL_GROUP = "signal_channel"

-- Internal state (equivalent to CsvModelDesc)
local csv_file
local csv_line
local csv_timestamp = -1
local signal_index = {}   -- csv_col -> sv.scalar index


-- Read next valid CSV data line (C: read_csv_line)
local function read_csv_line()
    csv_timestamp = -1

    while csv_timestamp < 0 do
        local line = csv_file:read("*l")
        if not line then
            return false
        end

        local ts = tonumber(line:match("^[^;]+"))
        if ts and ts >= 0 then
            csv_timestamp = ts
            csv_line = line
            return true
        end
    end
end


-- Build CSV column → signal index (C: vector index)
local function build_index(header)
    local col = 0
    for token in header:gmatch(CSV_DELIM_PATTERN) do
        col = col + 1
        if col > 1 then -- skip Timestamp
            local sv_idx = model.sv[SIGNAL_GROUP]:find(token)
            if sv_idx then
                signal_index[col] = sv_idx
                model:log_notice(
                    "indexed CSV column '%s' -> sv.scalar[%d]",
                    token, sv_idx
                )
            end
        end
    end
end


-- model_create (C: model_create)
function model_create()
    model:log_notice("model_create()")

    local csv_name = os.getenv(CSV_FILE_ENVAR)
    if not csv_name then
        model:log_error("CSV_FILE not set")
        return -1
    end

    csv_file = io.open(csv_name, "r")
    if not csv_file then
        model:log_error("Unable to open CSV file")
        return -1
    end

    -- Read header
    local header = csv_file:read("*l")
    if not header then
        model:log_error("CSV header missing")
        return -1
    end

    build_index(header)

    -- Preload first data line
    read_csv_line()

    return 0
end


-- model_step (C: model_step)
function model_step()
    local model_time = model:model_time()

    while csv_timestamp >= 0 and csv_timestamp <= model_time do
        local col = 0
        for value in csv_line:gmatch(CSV_DELIM_PATTERN) do
            col = col + 1
            if col > 1 then
                local sv_idx = signal_index[col]
                if sv_idx then
                    local v = tonumber(value)
                    if v then
                        model.sv[SIGNAL_GROUP].scalar[sv_idx] = v
                    end
                end
            end
        end

        if not read_csv_line() then
            break
        end
    end

    -- IMPORTANT:
    -- Do NOT advance model time manually in Lua
    return 0
end


-- model_destroy (C: model_destroy)
function model_destroy()
    model:log_notice("model_destroy()")
    if csv_file then
        csv_file:close()
    end
    return 0
end
