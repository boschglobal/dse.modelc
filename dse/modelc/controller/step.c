// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <assert.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/timer.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/ncodec/codec.h>


#define UNUSED(x) ((void)x)


typedef struct mf_step_data {
    ModelInstanceSpec* mi;
    double             model_time;
    double             stop_time;
} mf_step_data;


static int _do_step_func(void* _mf, void* _step_data)
{
    ModelFunction* mf = _mf;
    mf_step_data*  step_data = _step_data;
    ModelDesc*     md = step_data->mi->model_desc;

    for (SignalVector* sv = md->sv; sv && sv->name; sv++) {
        for (uint32_t i = 0; i < sv->count; i++) {
            if (sv->is_binary == false) continue;
            if (sv->ncodec[i] == NULL) continue;
            ncodec_seek(sv->ncodec[i], 0, NCODEC_SEEK_SET);
        }
    }

    double model_time = step_data->model_time;
    int    rc = md->vtable.step(md, &model_time, step_data->stop_time);
    if (rc)
        log_error(
            "Model Function %s:%s (rc=%d)", step_data->mi->name, mf->name, rc);

    return 0;
}


int step_model(ModelInstanceSpec* mi, double* model_time)
{
    ModelInstancePrivate* mip = mi->private;
    ControllerModel*      cm = mip->controller_model;
    AdapterModel*         am = mip->adapter_model;

    /* Step the Model (i.e. call registered Model Functions). */
    mf_step_data    step_data = { mi, am->model_time, am->stop_time };
    HashMap*        mf_map = &cm->model_functions;
    struct timespec stepcall_ts = get_timespec_now();
    int rc = hashmap_iterator(mf_map, _do_step_func, false, &step_data);
    am->bench_steptime_ns = get_elapsedtime_ns(stepcall_ts);

    /* Update the Model times. */
    am->model_time = am->stop_time;
    *model_time = am->model_time;
    return rc;
}


extern SignalValue* _get_signal_value(
    Channel* channel, const char* signal_name);

typedef struct merge_spec {
    ModelInstanceSpec* instptr;
    ModelInstanceSpec* target;
} merge_spec;

static Channel* _get_adapter_channel(ModelInstanceSpec* mi, const char* name)
{
    ModelInstancePrivate* mip = mi->private;
    AdapterModel*         am = mip->adapter_model;
    return hashmap_get(&am->channels, name);
}

static int _merge_backward_ch(void* _mfc, void* _spec)
{
    ModelFunctionChannel* mfc = _mfc;
    merge_spec*           spec = _spec;

    if (mfc->signal_value_double == NULL) return 0;  // Only scalar channels.

    /* Locate the channel on instptr. */
    Channel* ch_source = _get_adapter_channel(spec->instptr, mfc->channel_name);
    if (ch_source == NULL) return 0;

    /* Get the channel on the target. */
    Channel* ch_target = _get_adapter_channel(spec->target, mfc->channel_name);
    assert(ch_target);

    /* Merge from source -> target. */
    for (size_t i = 0; i < ch_target->index.count; i++) {
        SignalValue* sv_target = ch_target->index.map[i].signal;
        if (sv_target == NULL) continue;
        SignalValue* sv_source = _get_signal_value(ch_source, sv_target->name);
        if (sv_source == NULL) continue;

        sv_target->final_val = sv_source->final_val;
    }

    return 0;
}
static int _merge_backward_mf(void* _mf, void* _spec)
{
    ModelFunction* mf = _mf;
    return hashmap_iterator(&mf->channels, _merge_backward_ch, false, _spec);
}
static void _merge_scalar_signals_backward(
    SimulationSpec* sim, ModelInstanceSpec* target)
{
    /* Merge signals in the Adapter to the same (last) final_val.

    Because each Model may have different channels, its necessary to merge
    from all Model Instances to the target, ordered from left to right, so that
    the last updated value is the final value. */
    for (ModelInstanceSpec* _instptr = sim->instance_list;
        _instptr && _instptr->name; _instptr++) {
        if (_instptr != target) {
            ModelInstancePrivate* mip = target->private;
            ControllerModel*      cm = mip->controller_model;
            merge_spec            spec = { _instptr, target };
            hashmap_iterator(
                &cm->model_functions, _merge_backward_mf, false, &spec);
        }
    }
}

static int _merge_forward_ch(void* _mfc, void* _spec)
{
    ModelFunctionChannel* mfc = _mfc;
    merge_spec*           spec = _spec;

    if (mfc->signal_value_double == NULL) return 0;  // Only scalar channels.

    /* Locate the channel on instptr. */
    Channel* ch_source = _get_adapter_channel(spec->instptr, mfc->channel_name);
    if (ch_source == NULL) return 0;

    /* Get the channel on the target. */
    Channel* ch_target = _get_adapter_channel(spec->target, mfc->channel_name);
    assert(ch_target);

    /* Merge from source -> target. */
    for (size_t i = 0; i < ch_target->index.count; i++) {
        SignalValue* sv_target = ch_target->index.map[i].signal;
        if (sv_target == NULL) continue;
        SignalValue* sv_source = _get_signal_value(ch_source, sv_target->name);
        if (sv_source == NULL) continue;

        sv_target->val = sv_source->final_val;
    }

    return 0;
}
static int _merge_forward_mf(void* _mf, void* _spec)
{
    ModelFunction* mf = _mf;
    return hashmap_iterator(&mf->channels, _merge_forward_ch, false, _spec);
}
static void _merge_scalar_signals_forward(
    SimulationSpec* sim, ModelInstanceSpec* target)
{
    /* Merge signals to Adapter.

    Because each Model may have different channels, its necessary to merge
    from all previous Model Instances since a linear propagation (m1->m2->m3)
    would miss channels that were in m1 & m3 but not in m2. */
    for (ModelInstanceSpec* _instptr = sim->instance_list;
        _instptr && _instptr != target; _instptr++) {
        ModelInstancePrivate* mip = target->private;
        ControllerModel*      cm = mip->controller_model;
        merge_spec            spec = { _instptr, target };
        hashmap_iterator(&cm->model_functions, _merge_forward_mf, false, &spec);
    }
}


DLL_PRIVATE int sim_step_models(SimulationSpec* sim, double* model_time)
{
    assert(sim);
    int rc = 0;
    errno = 0;

    for (ModelInstanceSpec* _instptr = sim->instance_list;
        _instptr && _instptr->name; _instptr++) {
        if (sim->sequential_cosim) {
            /* Merge SignalValue->final_val values forward. */
            _merge_scalar_signals_forward(sim, _instptr);

            /* Transform signals into the Model. */
            marshal_model(_instptr, MARSHAL_ADAPTER2MODEL_SCALAR_ONLY);
        }
        rc = step_model(_instptr, model_time);
        if (rc) break;
        if (sim->sequential_cosim) {
            /* Transform signals from the Model (for subsequent merge). */
            marshal_model(_instptr, MARSHAL_MODEL2ADAPTER_SCALAR_ONLY);
        }
    }
    if (rc < 0) {
        log_error("An error occurred while in Step Handler");
        return 1;
    }
    if (rc > 0) {
        log_error("Model requested exit");
        return 2;
    }

    if (sim->sequential_cosim) {
        /* Merge SignalValue->final_val values backwards so that all
        instances of the SignalValue have the same (last) final_val (otherwise
        the resolution by the SimBus is non-determistic and the last value
        may not become the SimBus value). */
        for (ModelInstanceSpec* _instptr = sim->instance_list;
            _instptr && _instptr->name; _instptr++) {
            _merge_scalar_signals_backward(sim, _instptr);
        }
    }

    return 0;
}
