// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0


#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#define XXH_INLINE_ALL
#include <dse/clib/data/xxhash.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>
#include <dse/logger.h>


#define UNUSED(x) ((void)x)


typedef struct {
    ModelDesc model;

    /* Operation Parameters */
    uint32_t step_count;
    uint32_t model_count;
    uint32_t model_id;

    /* Block Parameters (calculated). */
    struct {
        SignalVector* sv;
        uint32_t      offset;
        uint32_t      size;
    } block;

    /* Checksum Signals. */
    struct {
        ModelSignalIndex sv_complete;
        ModelSignalIndex block_only;
    } checksum;
} BlockModelDesc;


static inline ModelSignalIndex _index_scalar(
    BlockModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = signal_index((ModelDesc*)m, v, s);
    if (idx.scalar == NULL) log_fatal("Signal not found (%s:%s)", v, s);
    return idx;
}


static inline uint32_t _get_envar(ModelDesc* m, const char* n, uint32_t val)
{
    char buf[50];

    snprintf(buf, sizeof(buf), "%s__%s", m->mi->name, n);
    for (size_t i = 0; i < strlen(buf); i++)
        buf[i] = toupper(buf[i]);
    if (getenv(buf)) {
        val = atoi(getenv(buf));
    } else {
        snprintf(buf, sizeof(buf), "%s", n);
        for (size_t i = 0; i < strlen(buf); i++)
            buf[i] = toupper(buf[i]);
        if (getenv(buf)) {
            val = atoi(getenv(buf));
        }
    }
    log_notice(LOG_COLOUR_LBLUE "  get_envar:%s=%u" LOG_COLOUR_NONE, buf, val);

    return val;
}


ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    BlockModelDesc* m = calloc(1, sizeof(BlockModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    /* Setup the parameters. */
    m->model_count = _get_envar((ModelDesc*)m, "MODEL_COUNT", 1);
    m->model_id = _get_envar((ModelDesc*)m, "MODEL_ID", 1);

    /* Calculate block parameters. */
    ModelSignalIndex idx = signal_index((ModelDesc*)m, "data", NULL);
    assert(idx.sv);
    m->block.sv = idx.sv;
    m->block.size = m->block.sv->count / m->model_count;
    m->block.offset = m->block.size * (m->model_id - 1);

    /* Setup checksum indexs. */
    char name[50];
    snprintf(name, sizeof(name), "checksum-%u", m->model_id);
    m->checksum.sv_complete = _index_scalar(m, "checksum", name);
    assert(m->checksum.sv_complete.scalar);
    snprintf(name, sizeof(name), "checksum-block-%u", m->model_id);
    m->checksum.block_only = _index_scalar(m, "checksum", name);
    assert(m->checksum.block_only.scalar);

    /* Document the conditions. */
    log_notice("m->block.sv->count %u", m->block.sv->count);
    log_notice("m->model_count %u", m->model_count);
    log_notice("m->model_id %u", m->model_id);
    log_notice("m->block.offset %u", m->block.offset);
    log_notice("m->block.size %u", m->block.size);

    /* Set the initial values. */
    for (size_t i = 0; i < m->block.size; i++) {
        size_t idx = m->block.offset + i;
        m->block.sv->scalar[idx] =
            (m->model_id * 10000) + m->block.offset + i + 1;
        log_trace("  %u:[%u] = %u", m->model_id, idx + 1,
            (uint32_t)m->block.sv->scalar[idx]);
    }

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    BlockModelDesc* m = (BlockModelDesc*)model;

    /* Calculate the checksum based on the entry state. */
    *m->checksum.sv_complete.scalar = XXH32(
        (void*)m->block.sv->scalar, m->block.sv->count * sizeof(double), 0);

    /* Increment this blocks signals. */
    for (size_t i = 0; i < m->block.size; i++) {
        size_t idx = m->block.offset + i;
        m->block.sv->scalar[idx] += 1;
        log_trace("  %u:[%u] = %u", m->model_id, idx + 1,
            (uint32_t)m->block.sv->scalar[idx]);
    }
    *m->checksum.block_only.scalar =
        XXH32((void*)&m->block.sv->scalar[m->block.offset],
            m->block.size * sizeof(double), 0);

    m->step_count += 1;
    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    BlockModelDesc* m = (BlockModelDesc*)model;
    log_notice(LOG_COLOUR_LBLUE "  %u:step count = %u" LOG_COLOUR_NONE,
        m->model_id, m->step_count);
    log_notice(LOG_COLOUR_LBLUE "  %u:%s = %u" LOG_COLOUR_NONE, m->model_id,
        m->checksum.sv_complete.sv->signal[m->checksum.sv_complete.signal],
        (uint32_t)*m->checksum.sv_complete.scalar);
    log_notice(LOG_COLOUR_LBLUE "  %u:%s = %u" LOG_COLOUR_NONE, m->model_id,
        m->checksum.block_only.sv->signal[m->checksum.block_only.signal],
        (uint32_t)*m->checksum.block_only.scalar);

    for (size_t i = 0; i < m->block.sv->count; i++) {
        size_t idx = i;
        log_trace("  %u:[%u] = %u", m->model_id, idx + 1,
            (uint32_t)m->block.sv->scalar[idx]);
    }
}
