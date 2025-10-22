// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdio.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/adapter/timer.h>


/*
Profiling and Benchmarks
========================

Fields
------
__ME__ (Model Execute Time)
: Time to complete the model do_step() function.

__MP__ (Model Processing Time)
: Time to respond to a Notify message (receive ModelStart then respond
  ModelReady) minus the Model Execute Time. This value represents the ModelC
  processing overhead.

__NET__ (Network Time)
: The amount of time taken to transmit and receive Notify messages. This value
  is calculated at the SimBus:
  Time(Notify RX) - Time(Notify TX) - (ME + MP)

__SW__ (SimBus Wait Time)
: The amount of time the SimBus algorithm waits before processing a particular
  models Notify data. The SimBus algorithm waits for _all_ models before it can
processes Notify data.

__SP__ (SimBus Processing Time)
: The time spent by the SimBus to process the incoming Notify messages. This
is a derived value:
Time(Model Total) = ME + MP + NET
Time(SP) = Total - Model Total - SW

__Total__ (Total Time)
: The total time.

*/


#define UNUSED(x)          ((void)x)
#define UINT32_STR_MAX_LEN 11


typedef struct ModelBenchmarkProfile {
    uint32_t        model_uid;
    struct timespec wait_ref_ts;

    /* Last sample. */
    uint64_t sam_model_execute_ns;
    uint64_t sam_model_proc_ns;
    uint64_t sam_network_ns;
    uint64_t sam_simbus_wait_ns;
    uint64_t sam_total_ns;

    /* Accumulators. */
    uint64_t acc_model_execute_ns;
    uint64_t acc_model_proc_ns;
    uint64_t acc_network_ns;
    uint64_t acc_simbus_wait_ns;
    uint64_t acc_total_ns;

    /* Averages. */
    uint32_t ma_sample_count;
    double   ma_model_execute;
    double   ma_model_proc;
    double   ma_network;
    double   ma_simbus_wait;
    double   ma_simbus_proc;
    double   ma_total;
} ModelBenchmarkProfile;


static HashMap  __model_data;
static uint32_t __accumulate_sample_count;
static uint32_t __accumulate_on_sample;


void simbus_profile_init(double bus_step_size)
{
    hashmap_init_alt(&__model_data, 64, NULL);
    __accumulate_on_sample = 1.0 / bus_step_size;
}


void simbus_profile_destroy(void)
{
    hashmap_destroy_ext(&__model_data, NULL, NULL);
}


static ModelBenchmarkProfile* _get_mbp(uint32_t model_uid)
{
    static char key[UINT32_STR_MAX_LEN];

    snprintf(key, UINT32_STR_MAX_LEN, "%u", model_uid);
    ModelBenchmarkProfile* mbp = hashmap_get(&__model_data, key);
    if (mbp == NULL) {
        mbp = calloc(1, sizeof(ModelBenchmarkProfile));
        mbp->model_uid = model_uid;
        hashmap_set(&__model_data, key, mbp);
    }
    return mbp;
}


static inline double _ns_to_us_to_sec(uint64_t t_ns)
{
    uint32_t t_us = t_ns / 1000;
    return t_us / 1000000.0;
}

static inline double _ma(double val, double mean, uint32_t n)
{
    double delta = val - mean;
    mean += delta / (double)n;
    return mean;
}

static void simbus_profile_update_averages(ModelBenchmarkProfile* mbp)
{
    if (__accumulate_sample_count == 0) return;

    /* Collect this accumulation cycle. */
    double model_execute = _ns_to_us_to_sec(mbp->acc_model_execute_ns);
    double model_proc = _ns_to_us_to_sec(mbp->acc_model_proc_ns);
    double network = _ns_to_us_to_sec(mbp->acc_network_ns);
    double simbus_wait = _ns_to_us_to_sec(mbp->acc_simbus_wait_ns);
    double total = _ns_to_us_to_sec(mbp->acc_total_ns);
    double model_total = model_execute + model_proc + network;
    double simbus_proc = mbp->ma_total - model_total - mbp->ma_simbus_wait;
    if (__accumulate_sample_count < __accumulate_on_sample) {
        /* Normalise the accumulator samples to full 1.0 second. */
        double factor = __accumulate_on_sample / __accumulate_sample_count;
        model_execute *= factor;
        model_proc *= factor;
        network *= factor;
        simbus_wait *= factor;
        total *= factor;
        model_total *= factor;
        simbus_proc *= factor;
    }

    /* Moving average according to Welford's algorithm. */
    mbp->ma_sample_count++;
    if (mbp->ma_sample_count == 1) {
        mbp->ma_model_execute = model_execute;
        mbp->ma_model_proc = model_proc;
        mbp->ma_network = network;
        mbp->ma_simbus_wait = simbus_wait;
        mbp->ma_total = total;
        model_total =
            mbp->ma_model_execute + mbp->ma_model_proc + mbp->ma_network;
        mbp->ma_simbus_proc = mbp->ma_total - model_total - mbp->ma_simbus_wait;
    } else {
        uint32_t n = mbp->ma_sample_count;
        mbp->ma_model_execute = _ma(model_execute, mbp->ma_model_execute, n);
        mbp->ma_model_proc = _ma(model_proc, mbp->ma_model_proc, n);
        mbp->ma_network = _ma(network, mbp->ma_network, n);
        mbp->ma_simbus_wait = _ma(simbus_wait, mbp->ma_simbus_wait, n);
        mbp->ma_total = _ma(total, mbp->ma_total, n);
        model_total =
            mbp->ma_model_execute + mbp->ma_model_proc + mbp->ma_network;
        mbp->ma_simbus_proc = mbp->ma_total - model_total - mbp->ma_simbus_wait;
    }
}


void simbus_profile_accumulate_model_part(uint32_t model_uid, uint64_t me,
    uint64_t mp, uint64_t net, struct timespec ref_ts)
{
    ModelBenchmarkProfile* mbp = _get_mbp(model_uid);

    /* Sample. */
    mbp->sam_model_execute_ns = me;
    mbp->sam_model_proc_ns = mp;
    mbp->sam_network_ns = net;

    /* Accumulate. */
    mbp->acc_model_execute_ns += me;
    mbp->acc_model_proc_ns += mp;
    mbp->acc_network_ns += net;

    /* Set reference time for simbus part. */
    mbp->wait_ref_ts = ref_ts;
}


typedef struct SimbusCycleData {
    uint64_t        cycle_total_ns;
    struct timespec ref_ts;
} SimbusCycleData;

static int _acc_simbus_part(void* map_item, void* additional_data)
{
    ModelBenchmarkProfile* mbp = map_item;
    SimbusCycleData*       data = additional_data;
    uint64_t simbus_wait_ns = get_deltatime_ns(mbp->wait_ref_ts, data->ref_ts);

    /* Accumulate. */
    mbp->sam_simbus_wait_ns = simbus_wait_ns;
    mbp->sam_total_ns = data->cycle_total_ns;

    /* Accumulate. */
    mbp->acc_simbus_wait_ns += simbus_wait_ns;
    mbp->acc_total_ns += data->cycle_total_ns;
    __accumulate_sample_count++;

    /* Averages. */
    if (__accumulate_sample_count >= __accumulate_on_sample) {
        simbus_profile_update_averages(mbp);
        /* Reset accumulators. */
        mbp->acc_model_execute_ns = 0;
        mbp->acc_model_proc_ns = 0;
        mbp->acc_network_ns = 0;
        mbp->acc_simbus_wait_ns = 0;
        mbp->acc_total_ns = 0;
        __accumulate_sample_count = 0;
    }

    return 0;
}

void simbus_profile_accumulate_cycle_total(
    uint64_t simbus_cycle_total_ns, struct timespec ref_ts)
{
    SimbusCycleData data = {
        .cycle_total_ns = simbus_cycle_total_ns,
        .ref_ts = ref_ts,
    };
    hashmap_iterator(&__model_data, _acc_simbus_part, false, &data);
}


static int _print_benchmark(void* map_item, void* additional_data)
{
    UNUSED(additional_data);
    ModelBenchmarkProfile* mbp = map_item;
    simbus_profile_update_averages(mbp);
    log_notice("  %-9u  %-11.6f %-11.6f %-11.6f %-11.6f %-8.3f %-8.3f",
        mbp->model_uid, mbp->ma_model_execute, mbp->ma_model_proc,
        mbp->ma_network, mbp->ma_simbus_wait, mbp->ma_simbus_proc,
        mbp->ma_total);
    return 0;
}

static int _print_benchmark_acc(void* map_item, void* additional_data)
{
    UNUSED(additional_data);
    ModelBenchmarkProfile* mbp = map_item;

    double model_execute = _ns_to_us_to_sec(mbp->acc_model_execute_ns);
    double model_proc = _ns_to_us_to_sec(mbp->acc_model_proc_ns);
    double network = _ns_to_us_to_sec(mbp->acc_network_ns);
    double simbus_wait = _ns_to_us_to_sec(mbp->acc_simbus_wait_ns);
    double total = _ns_to_us_to_sec(mbp->acc_total_ns);
    double model_total = model_execute + model_proc + network;
    double simbus_proc = total - model_total - simbus_wait;

    log_notice("  %-9u  %-11.6f %-11.6f %-11.6f %-11.6f %-8.3f %-8.3f",
        mbp->model_uid, model_execute, model_proc, network, simbus_wait,
        simbus_proc, total);
    return 0;
}

static int _print_benchmark_sam(void* map_item, void* additional_data)
{
    UNUSED(additional_data);
    ModelBenchmarkProfile* mbp = map_item;

    double model_execute = _ns_to_us_to_sec(mbp->sam_model_execute_ns);
    double model_proc = _ns_to_us_to_sec(mbp->sam_model_proc_ns);
    double network = _ns_to_us_to_sec(mbp->sam_network_ns);
    double simbus_wait = _ns_to_us_to_sec(mbp->sam_simbus_wait_ns);
    double total = _ns_to_us_to_sec(mbp->sam_total_ns);
    double model_total = model_execute + model_proc + network;
    double simbus_proc = total - model_total - simbus_wait;

    log_notice("  %-9u  %-11.6f %-11.6f %-11.6f %-11.6f %-8.3f %-8.3f",
        mbp->model_uid, model_execute, model_proc, network, simbus_wait,
        simbus_proc, total);
    return 0;
}

void simbus_profile_print_benchmarks(void)
{
    log_notice("Profile/Benchmark Data:");
    log_notice(" Normalised: (relative to 1.0 second simulation time)");
    log_notice("  model_uid  ME          MP          NET         SW          "
               "SP       Total");
    hashmap_iterator(&__model_data, _print_benchmark, false, NULL);
    log_notice(" Accumulators: (raw accumulated sample data)");
    log_notice("  model_uid  ME          MP          NET         SW          "
               "SP       Total");
    hashmap_iterator(&__model_data, _print_benchmark_acc, false, NULL);
    log_notice(" Samples: (last sample data)");
    log_notice("  model_uid  ME          MP          NET         SW          "
               "SP       Total");
    hashmap_iterator(&__model_data, _print_benchmark_sam, false, NULL);
}
