// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>
#include <dse/logger.h>


static struct timespec _get_timespec_now(void)
{
    struct timespec ts = {};
    clock_gettime(CLOCK_SOURCE, &ts);
    return ts;
}


static uint64_t _get_elapsedtime_us(struct timespec ref)
{
    struct timespec now = _get_timespec_now();
    uint64_t        u_sec = (now.tv_sec * 1000000 + now.tv_nsec / 1000) -
                     (ref.tv_sec * 1000000 + ref.tv_nsec / 1000);
    return u_sec;
}


typedef struct {
    ModelDesc model;

    /* Operation Parameters */
    uint32_t model_id;

    /* Block Parameters (calculated). */
    struct {
        SignalVector* sv;
        uint32_t      offset;
        uint32_t      size;
    } block;

    /* Profiling data. */
    struct timespec start_time;
    uint64_t        setup_time;
    uint64_t        step_count;
} ExtendedModelDesc;


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
    ExtendedModelDesc* m = calloc(1, sizeof(ExtendedModelDesc));
    memcpy(m, model, sizeof(ModelDesc));
    m->start_time = _get_timespec_now();

    /* Setup the parameters. */
    m->model_id = _get_envar((ModelDesc*)m, "MODEL_ID", 1);
    m->block.size = _get_envar((ModelDesc*)m, "SIGNAL_CHANGE", 5);
    m->block.sv = m->model.sv;
    m->block.offset = m->block.size * (m->model_id - 1);
    if ((m->block.offset + m->block.size) > m->model.sv->count) {
        m->block.offset = 0;
    }

    /* Document the conditions. */
    log_notice("m->model_id %u", m->model_id);
    log_notice("m->block.offset %u", m->block.offset);
    log_notice("m->block.size %u", m->block.size);
    log_notice("m->block.sv->count %u", m->block.sv->count);

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;
    if (*model_time == 0) {
        m->setup_time = _get_elapsedtime_us(m->start_time);
    }

    for (size_t i = 0; i < m->block.size; i++) {
        size_t idx = m->block.offset + i;
        m->block.sv->scalar[idx] += 1;
    }

    m->step_count += 1;
    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;

    const char* tag = "";
    if (getenv("TAG")) tag = getenv("TAG");
    log_notice(LOG_COLOUR_LBLUE
        ":::benchmark:%s::%s;%s;%.6f;%.6f;%u;%u;%u;%lu;%."
        "3f;%.3f:::" LOG_COLOUR_NONE,
        tag, m->model.sim->transport, m->model.sim->uri,
        m->model.sim->step_size, m->model.sim->end_time, m->model_id,
        m->model.sv->count, m->block.size, m->step_count,
        m->setup_time / 1000000.0,
        _get_elapsedtime_us(m->start_time) / 1000000.0);
}
