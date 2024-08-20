// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>


static inline uint64_t _get_seed()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
}


static inline uint32_t _get_signal_change(
    SignalVector* sv, uint32_t limit, uint32_t* seed)
{
    if (limit == 1) return limit;

    uint32_t minimum = 1;
    uint32_t maximum = (limit > sv->count) ? sv->count : limit;
    /* 1 .. 1000 -> 1 .. rand_r(change) */
    return (rand_r(seed) % (maximum + 1 - minimum)) + minimum;
}


typedef struct {
    ModelDesc model;
    /* Benchmark parameters. */
    uint32_t  signal_change;
    uint32_t  change_seed;
} ExtendedModelDesc;


ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    ExtendedModelDesc* m = calloc(1, sizeof(ExtendedModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    /* Setup the benchmark parameters. */
    m->signal_change = 1;
    if (getenv("SIGNAL_CHANGE")) {
        m->signal_change = atoi(getenv("SIGNAL_CHANGE"));
    }
    m->change_seed = _get_seed();

    log_notice("signal count  : %d", m->model.sv->count);
    log_notice("signal change : %d", m->signal_change);
    log_notice("seed value    : %d", m->change_seed);

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;
    for (SignalVector* sv = m->model.sv; sv->name; sv++) {
        uint32_t count =
            _get_signal_change(sv, m->signal_change, &m->change_seed);
        log_info("Signal change count is : %d", count);
        for (size_t i = 0; i < count; i++) {
            sv->scalar[i] += 1.2;
        }
    }
    *model_time = stop_time;
    return 0;
}
