// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>
#include <dse/logger.h>


static uint64_t _get_seed()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
}


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


static uint32_t _get_signal_change(
    SignalVector* sv, uint32_t limit, uint32_t* seed)
{
    if (limit == 1) return limit;

    uint32_t minimum = 1;
    uint32_t maximum = (limit > sv->count) ? sv->count : limit;
    /* 1 .. 1000 -> 1 .. rand_r(change) */
    return (rand_r(seed) % (maximum + 1 - minimum)) + minimum;
}


typedef struct {
    ModelDesc       model;
    /* Benchmark parameters. */
    uint32_t        signal_change;
    uint32_t        change_seed;
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

    /* Setup the benchmark parameters. */
    m->signal_change =
        _get_envar((ModelDesc*)m, "SIGNAL_CHANGE", m->model.sv->count);
    m->change_seed = _get_seed();

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;
    if (*model_time == 0) {
        m->setup_time = _get_elapsedtime_us(m->start_time);
    }

    for (SignalVector* sv = m->model.sv; sv->name; sv++) {
        uint32_t count =
            _get_signal_change(sv, m->signal_change, &m->change_seed);
        log_info("Signal change count is : %d", count);
        for (size_t i = 0; i < count; i++) {
            sv->scalar[i] += 1.2;
        }
    }
    m->step_count += 1;

    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;

    log_error(LOG_COLOUR_LBLUE "::benchmark:%s::%s;%s;%.6f;%.6f;%u;%u;%u;%lu;%."
                               "3f;%.3f" LOG_COLOUR_NONE,
        getenv("TAG"), m->model.sim->transport, m->model.sim->uri,
        m->model.sim->step_size, m->model.sim->end_time, m->model.mi->uid,
        m->model.sv->count, m->signal_change, m->step_count,
        m->setup_time / 1000000.0,
        _get_elapsedtime_us(m->start_time) / 1000000.0);
}
