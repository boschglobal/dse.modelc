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
            SimpleSet ex_signals;
            set_init(&ex_signals);
            MarshalMapSpec source = {
                .count = *model->source.count,
                .scalar = model->source.scalar,
                .signal = model->source.signal,
            };
            for (SignalVector* sv = model->model.sv; sv && sv->name; sv++) {
                MarshalMapSpec signal = {
                    .count = sv->count,
                    .name = sv->name,
                    .scalar = &(sv->scalar),
                    .signal = sv->signal,
                };
                MarshalSignalMap* msm = marshal_generate_signalmap(
                    signal, source, &ex_signals, sv->is_binary);
                if (errno != 0) return errno;
                hashlist_append(&msm_list, msm);

                /* Logging. */
                log_notice("FMU <-> SignalVector mapping for: %s", msm->name);
                for (uint32_t i = 0; i < msm->count; i++) {
                    log_notice("  Variable: %s", sv->signal[msm->signal.index[i]]);
                }
            }
            set_destroy(&ex_signals);

            /* Convert to a NTL. */
            size_t count = hashlist_length(&msm_list);
            model->msm = calloc(count + 1, sizeof(MarshalSignalMap));
            for (uint32_t i = 0; i < count; i++) {
                memcpy(&model->msm[i], hashlist_at(&msm_list, i), sizeof(MarshalSignalMap));
                free(hashlist_at(&msm_list, i));
            }
            hashlist_destroy(&msm_list);
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
    int rc;

    if (model && model->vtable.step) {
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
            if (model_stop_time > end_time) return 0;

            rc = model->vtable.step(model, &model_current_time, model_stop_time);
            model->model_time = model_current_time;
        } while (model_stop_time < end_time);

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
