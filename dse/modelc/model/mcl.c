// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdlib.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/clib/data/marshal.h>
#include <dse/modelc/mcl.h>



/**
mcl_create
==========

Create an instance of the MCL which will then be used to operate the Model that
the MCL represents.

> Implemented by MCL.

Parameters
----------
model (ModelDesc*)
: Model descriptor object.

Returns
-------
MclDesc (pointer)
: Object representing the MCL Model, an extended ModelDesc type (derived from
parameter `model`).

NULL
: The MCL Model could not be created. Inspect `errno` for more details.

Error Conditions
----------------

Available by inspection of `errno`.
*/
extern MclDesc* mcl_create(ModelDesc* model);


/**
mcl_destroy
===========

Releases memory and system resources allocated by `mcl_create()`.

> Implemented by MCL.

Parameters
----------
model (ModelDesc*)
: Model descriptor object.

*/
extern void mcl_destroy(MclDesc* model);


static void _match_block(MclDesc* model, HashList* msm_list, size_t offset,
    size_t count, MarshalKind kind)
{
    if (count == 0) return;

    /* Construct the source references. */
    MarshalMapSpec source;
    switch (kind) {
    case MARSHAL_KIND_PRIMITIVE:
        source = (MarshalMapSpec){
            .count = count,
            .is_binary = false,
            .signal = &model->source.signal[offset],
            .scalar = &model->source.scalar[offset],
        };
        break;
    case MARSHAL_KIND_BINARY:
        source = (MarshalMapSpec){
            .count = count,
            .is_binary = true,
            .signal = &model->source.signal[offset],
            .binary = &model->source.binary[offset],
            .binary_len = &model->source.binary_len[offset],
        };
        break;
    default:
        return;
    }

    for (SignalVector* sv = model->model.sv; sv && sv->name; sv++) {
        if (sv->is_binary != source.is_binary) continue;
        /* Construct the signal reference. */
        MarshalMapSpec signal;
        if (sv->is_binary) {
            signal = (MarshalMapSpec){
                .name = sv->name,
                .count = sv->count,
                .is_binary = true,
                .signal = sv->signal,
                .binary = sv->binary,
                .binary_len = sv->length,
                .binary_buffer_size = sv->buffer_size,
            };
        } else {
            signal = (MarshalMapSpec){
                .name = sv->name,
                .count = sv->count,
                .signal = sv->signal,
                .scalar = sv->scalar,
            };
        }
        /* Generate the map. */
        errno = 0;
        MarshalSignalMap* msm =
            marshal_generate_signalmap(signal, source, NULL, sv->is_binary);
        if (errno != 0) {
            if (msm) free(msm);
            return;
        }
        hashlist_append(msm_list, msm);

        /* Logging. */
        log_notice("SignalVector <-> MCL mapping for: %s", msm->name);
        for (uint32_t i = 0; i < msm->count; i++) {
            log_notice("  Signal: %s (%d) <-> %s (%d)",
                sv->signal[msm->signal.index[i]], msm->signal.index[i],
                model->source.signal[msm->source.index[i]],
                msm->source.index[i]);
        }
    }
}


/**
mcl_load
========

This method calls the MCL `load()` method which in turn loads the Model being
represented by this MCL instance.

Parameters
----------
model (MclDesc*)
: The MCL Descriptor object representing an instance of the MCL Model.

Returns
-------
0 (int32_t)
: The related model was loaded by the MCL.

-EINVAL (-22)
: Bad `model` argument.
*/
int32_t mcl_load(MclDesc* model)
{
    if (model && model->vtable.load) {
        if (model->model.sv) {
            errno = 0;
            HashList msm_list;
            hashlist_init(&msm_list, 64);

            /* Parse the source for blocks of similar Kind. */
            MarshalKind kind = MARSHAL_KIND_NONE;
            size_t      offset = 0;
            size_t      count = 0;
            for (size_t i = 0; i < model->source.count; i++) {
                if (model->source.kind[i] != kind) {
                    /* New block detected, emit the previous block. */
                    _match_block(model, &msm_list, offset, count, kind);
                    /* Setup for the new block (this item). */
                    offset = i;
                    count = 1;
                    kind = model->source.kind[i];
                } else {
                    /* Current block. */
                    count++;
                }
            }
            _match_block(model, &msm_list, offset, count, kind);
            /* Convert to a NTL. */
            model->msm =
                hashlist_ntl(&msm_list, sizeof(MarshalSignalMap), true);
        }

        return model->vtable.load(model);
    } else {
        return -EINVAL;
    }
}


/**
mcl_init
========

This method calls the MCL `init()` method which initialises the Model being
represented by this MCL instance.

Parameters
----------
model (MclDesc*)
: The MCL Descriptor object representing an instance of the MCL Model.

Returns
-------
0 (int32_t)
: The represented model was initialised by the MCL.

-EINVAL (-22)
: Bad `model` argument.
*/
int32_t mcl_init(MclDesc* model)
{
    if (model && model->vtable.init) {
        return model->vtable.init(model);
    } else {
        return -EINVAL;
    }
}


/**
mcl_step
========

This method calls the MCL `step()` method to advance the Model represented by
this MCL instance to the simulation time specifed by the parameter `end_time`.

Parameters
----------
model (MclDesc*)
: The MCL Descriptor object representing an instance of the MCL Model.

Returns
-------
0 (int32_t)
: The represented model was stepped by the MCL to the specified `end_time`.

-1
: The model time is advanced beyond the current simulation time.

-EINVAL (-22)
: Bad `model` argument.
*/
int32_t mcl_step(MclDesc* model, double end_time)
{
    double model_stop_time;
    double model_current_time;
    double mcl_epsilon = 0.0;
    int    rc;

    if (model && model->vtable.step) {
        /* Reset binary signals. */
        for (SignalVector* sv = model->model.sv; sv && sv->name; sv++) {
            if (sv->is_binary) {
                for (uint32_t i = 0; i < sv->count; i++) {
                    signal_reset(sv, i);
                }
            }
        }

        /* Calculate epsilon value (if necessary). */
        if (model->step_size) mcl_epsilon = model->step_size * 0.01;

        do {
            /* Determine times. */
            model_current_time = model->model_time;
            model_stop_time = end_time;

            if (model_current_time >= model_stop_time) {
                return -1;
            }
            if (model->step_size) {
                /**
                 *  Use the annotated step size.
                 *  Increment via Kahan summation.
                 *
                 *  model_stop_time = model_time + model->step_size;
                 */
                double y = model->step_size - model->model_time_correction;
                double t = model_current_time + y;
                model->model_time_correction = (t - model_current_time) - y;
                model_stop_time = t;
            }
            /* Model stop time past Simulation stop time. */
            if (model_stop_time > end_time + mcl_epsilon) return 0;

            log_trace("Step the FMU Model: %f %f (%f) %f", model_current_time,
                model_stop_time, (model_stop_time + mcl_epsilon), end_time);

            rc =
                model->vtable.step(model, &model_current_time, model_stop_time);
            model->model_time = model_current_time;

            // FIXME: when MCL Target is stepped multiple times, it will be
            // necessary to marshal in binary signals after each step.
        } while (model_stop_time + mcl_epsilon < end_time);

        return rc;
    } else {
        return -EINVAL;
    }
}


/**
mcl_marshal_out
===============

This method calls the MCL `marshal_out()` method which in turn marshals
signals outwards, towards the Model represented by this MCL instance.

Parameters
----------
model (MclDesc*)
: The MCL Descriptor object representing an instance of the MCL Model.

Returns
-------
0 (int32_t)
: Signals were marshalled outwards to the Model represented by the MCL.

-EINVAL (-22)
: Bad `model` argument.
*/
int32_t mcl_marshal_out(MclDesc* model)
{
    if (model && model->vtable.marshal_out) {
        marshal_signalmap_out(model->msm);
        return model->vtable.marshal_out(model);
    } else {
        return -EINVAL;
    }
}


/**
mcl_marshal_in
==============

This method calls the MCL `marshal_in()` method which in turn marshals
signals inwards, from the Model represented by this MCL instance.

Parameters
----------
model (MclDesc*)
: The MCL Descriptor object representing an instance of the MCL Model.

Returns
-------
0 (int32_t)
: Signals were marshalled inwards from the Model represented by the MCL.

-EINVAL (-22)
: Bad `model` argument.
*/
int32_t mcl_marshal_in(MclDesc* model)
{
    if (model && model->vtable.marshal_in) {
        int32_t rc = model->vtable.marshal_in(model);
        if (rc != 0) return rc;
        marshal_signalmap_in(model->msm);
        return rc;
    } else {
        return -EINVAL;
    }
}


/**
mcl_unload
==========

This method calls the MCL `unload()` method which unloads the Model
represented by this MCL instance.

Parameters
----------
model (MclDesc*)
: The MCL Descriptor object representing an instance of the MCL Model.

Returns
-------
0 (int32_t)
: The represented model was unloaded by the MCL.

-EINVAL (-22)
: Bad `model` argument.
*/
int32_t mcl_unload(MclDesc* model)
{
    if (model && model->vtable.unload) {
        for (MarshalSignalMap* msm = model->msm; msm && msm->name; msm++) {
            if (msm->signal.index) free(msm->signal.index);
            if (msm->source.index) free(msm->source.index);
        }
        if (model->msm) free(model->msm);
        return model->vtable.unload(model);
    } else {
        return -EINVAL;
    }
}
