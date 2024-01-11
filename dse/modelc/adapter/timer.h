// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_ADAPTER_TIMER_H_
#define DSE_MODELC_ADAPTER_TIMER_H_


#include <time.h>
#include <stdint.h>
#include <dse/platform.h>


inline struct timespec get_timespec_now(void)
{
    struct timespec ts = {};
    clock_gettime(CLOCK_SOURCE, &ts);
    return ts;
}


inline uint64_t get_elapsedtime_us(struct timespec ref)
{
    struct timespec now = get_timespec_now();
    uint64_t        u_sec = (now.tv_sec * 1000000 + now.tv_sec / 1000) -
                     (ref.tv_sec * 1000000 + ref.tv_sec / 1000);
    return u_sec;
}


inline uint64_t get_elapsedtime_ns(struct timespec ref)
{
    struct timespec now = get_timespec_now();
    if (ref.tv_sec == now.tv_sec) {
        return now.tv_nsec - ref.tv_nsec;
    } else {
        return ((now.tv_sec - ref.tv_sec) * 1000000000) +
               (now.tv_nsec - ref.tv_nsec);
    }
}


inline uint64_t get_deltatime_ns(
    struct timespec from_ref, struct timespec to_ref)
{
    if (from_ref.tv_sec == to_ref.tv_sec) {
        return to_ref.tv_nsec - from_ref.tv_nsec;
    } else {
        return ((to_ref.tv_sec - from_ref.tv_sec) * 1000000000) +
               (to_ref.tv_nsec - from_ref.tv_nsec);
    }
}


#endif  // DSE_MODELC_ADAPTER_TIMER_H_
