// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <dlfcn.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/controller/model_private.h>


#define UNUSED(x) ((void)x)


static Controller* __controller = NULL;


DLL_PRIVATE Controller* controller_object_ref(void)
{
    return __controller;
}


void controller_destroy(void)
{
    Controller* controller = __controller;
    if (controller == NULL) return;

    if (controller->adapter) adapter_destroy(controller->adapter);

    free(__controller);
    __controller = NULL;
}


int controller_init(Endpoint* endpoint)
{
    assert(__controller == NULL);

    errno = 0;
    __controller = calloc(1, sizeof(Controller));
    if (__controller == NULL) {
        log_error("Controller malloc failed!");
        goto error_clean_up;
    }

    Controller* controller = __controller;
    controller->stop_request = false;

    log_notice("Create the Adapter object ...");
    controller->adapter = adapter_create(endpoint);
    if (controller->adapter == NULL) {
        if (errno == 0) errno = EINVAL;
        log_error("Adapter create failed!");
        goto error_clean_up;
    }

    return 0;

error_clean_up:
    controller_destroy();
    return -1;
}


int controller_init_channel(ModelInstanceSpec* model_instance,
    const char* channel_name, const char** signal_name, uint32_t signal_count)
{
    assert(model_instance);
    ModelInstancePrivate* mip = model_instance->private;
    AdapterModel*         am = mip->adapter_model;

    log_notice("Init Controller channel: %s", channel_name);
    adapter_init_channel(am, channel_name, signal_name, signal_count);

    return 0;
}


static int __marshal__adapter2model(void* _mfc, void* _spec)
{
    ModelFunctionChannel*  mfc = _mfc;
    ControllerMarshalSpec* spec = _spec;
    ModelInstancePrivate*  mip = spec->mi->private;
    AdapterModel*          am = mip->adapter_model;

    if (mfc->signal_map == NULL) {
        mfc->signal_map = adapter_get_signal_map(
            am, mfc->channel_name, mfc->signal_names, mfc->signal_count);
    }
    SignalMap* sm = mfc->signal_map;

    if (mfc->signal_value_double) {
        controller_transform_to_model(mfc, sm);
    }
    if (spec->dir == MARSHAL_ADAPTER2MODEL_SCALAR_ONLY) return 0;

    if (mfc->signal_value_binary) {
        for (uint32_t si = 0; si < mfc->signal_count; si++) {
            dse_buffer_append(&mfc->signal_value_binary[si],
                &mfc->signal_value_binary_size[si],
                &mfc->signal_value_binary_buffer_size[si], sm[si].signal->bin,
                sm[si].signal->bin_size);
            /* Indicate the binary object was consumed. */
            sm[si].signal->bin_size = 0;
            /* Set the trigger to detect if the binary object is correctly
               operated by the Model (i.e. calls reset()).*/
            mfc->signal_value_binary_reset_called[si] = false;
        }
    }
    return 0;
}
static int __marshal__model2adapter(void* _mfc, void* _spec)
{
    ModelFunctionChannel*  mfc = _mfc;
    ControllerMarshalSpec* spec = _spec;
    ModelInstancePrivate*  mip = spec->mi->private;
    AdapterModel*          am = mip->adapter_model;

    if (mfc->signal_map == NULL) {
        mfc->signal_map = adapter_get_signal_map(
            am, mfc->channel_name, mfc->signal_names, mfc->signal_count);
    }
    SignalMap* sm = mfc->signal_map;

    if (spec->dir == MARSHAL_MODEL2ADAPTER ||
        spec->dir == MARSHAL_MODEL2ADAPTER_SCALAR_ONLY) {
        if (mfc->signal_value_double) {
            controller_transform_from_model(mfc, sm);
        }
    }

    if (spec->dir == MARSHAL_MODEL2ADAPTER ||
        spec->dir == MARSHAL_MODEL2ADAPTER_BINARY_ONLY) {
        if (mfc->signal_value_binary) {
            for (uint32_t si = 0; si < mfc->signal_count; si++) {
                if (mfc->signal_value_binary_reset_called[si] == false) {
                    /* Force size to 0.
                    Expected operation is: read, reset, write (append).
                    If reset is not called, i.e. the Model does not consume
                    _this_ signal, then data will be echo'ed back. If more than
                    one model echoes data back, the SimBus may also echo back
                    and ever increasing about of data. */
                    mfc->signal_value_binary_size[si] = 0;
                }
                dse_buffer_append(&sm[si].signal->bin, &sm[si].signal->bin_size,
                    &sm[si].signal->bin_buffer_size,
                    mfc->signal_value_binary[si],
                    mfc->signal_value_binary_size[si]);
                /* Indicate the binary object was consumed. */
                mfc->signal_value_binary_size[si] = 0;
            }
        }
    }
    return 0;
}
static int __marshal__model_function(void* _mf, void* _spec)
{
    ModelFunction*         mf = _mf;
    ControllerMarshalSpec* spec = _spec;
    int                    rc = 0;
    switch (spec->dir) {
    case MARSHAL_ADAPTER2MODEL:
    case MARSHAL_ADAPTER2MODEL_SCALAR_ONLY:
        rc = hashmap_iterator(
            &mf->channels, __marshal__adapter2model, false, spec);
        break;
    case MARSHAL_MODEL2ADAPTER:
    case MARSHAL_MODEL2ADAPTER_SCALAR_ONLY:
    case MARSHAL_MODEL2ADAPTER_BINARY_ONLY:
        rc = hashmap_iterator(
            &mf->channels, __marshal__model2adapter, false, spec);
        break;
    default:
        break;
    }
    return rc;
}
static int __marshal__model(ControllerModel* cm, void* spec)
{
    int rc = 0;
    rc = hashmap_iterator(
        &cm->model_functions, __marshal__model_function, false, spec);
    return rc;
}

void marshal_model(ModelInstanceSpec* mi, ControllerMarshalDir dir)
{
    ControllerMarshalSpec md = { dir, mi };
    ModelInstancePrivate* mip = mi->private;
    ControllerModel*      cm = mip->controller_model;
    __marshal__model(cm, &md);
}

static void marshal(SimulationSpec* sim, ControllerMarshalDir dir)
{
    assert(sim);
    for (ModelInstanceSpec* _instptr = sim->instance_list;
         _instptr && _instptr->name; _instptr++) {
        marshal_model(_instptr, dir);
    }
}


void controller_bus_ready(SimulationSpec* sim)
{
    assert(__controller);
    Controller* controller = __controller;

    /* Explicitly start the endpoint (creates resources etc). */
    assert(controller->adapter);
    Adapter* adapter = controller->adapter;
    assert(adapter->endpoint);
    Endpoint* endpoint = adapter->endpoint;
    if (endpoint->start) endpoint->start(endpoint);

    /* Connect with the bus. */
    adapter_connect(adapter, sim, 5);
    if (controller->stop_request) return;

    /* Register the signals with the bus. */
    adapter_register(adapter, sim);
}


int controller_step(SimulationSpec* sim)
{
    assert(__controller);
    Controller* controller = __controller;
    assert(controller->adapter);
    Adapter* adapter = controller->adapter;

    int rc;

    /* Marshal data from Model Functions to Adapter Channels. */
    if (sim->sequential_cosim) {
        /* The scalar final_val are already resolved in sim_step_models() so
        only marshal out the binary signals. */
        marshal(sim, MARSHAL_MODEL2ADAPTER_BINARY_ONLY);
    } else {
        marshal(sim, MARSHAL_MODEL2ADAPTER);
    }

    /* ModelReady and wait on ModelStart.

        Possible error conditions:
        ETIME : Timeout while waiting for ModelStart. May indicate that another
            model has left the Simulation (e.g. Standalone Simbus when no Agents
            are present to change model registration count).

        Caller can attempt a clean exit from the Simulation (i.e. send
       ModelExit).
    */
    rc = adapter_ready(adapter, sim);
    if (rc) return rc;

    /* Marshal data from Adapter Channels to Model Functions. */
    marshal(sim, MARSHAL_ADAPTER2MODEL);


    /* Model callbacks.
     * These notify the model of the _next_ start and stop time, which the
     * model should use for its "async" execution. After that execution the
     * model will call modelc_controller_sync() which will call this method
     * to update the SimBus based on those start/end times. */
    double end_time = sim->end_time;
    double model_time = sim->end_time;
    rc = sim_step_models(sim, &model_time);
    if (rc) return rc;

    /* End condition? */
    if (end_time > 0 && end_time < model_time) return 1;
    /* Otherwise, return 0 indicating that do_step was successful. */
    return 0;
}


int controller_step_phased(SimulationSpec* sim)
{
    assert(__controller);
    Controller* controller = __controller;
    assert(controller->adapter);
    Adapter* adapter = controller->adapter;
    int      rc;

    /* Pull data from SimBus. */
    rc = adapter_model_start(adapter, sim); /* Causes time to progress. */
    if (rc) return rc;
    marshal(sim, MARSHAL_ADAPTER2MODEL);

    /* Model callbacks.
     * These notify the model of the _next_ start and stop time, which the
     * model should use for its "async" execution. After that execution the
     * model will call modelc_controller_sync() which will call this method
     * to update the SimBus based on those start/end times. */
    double end_time = sim->end_time;
    double model_time = sim->end_time;
    rc = sim_step_models(sim, &model_time);
    if (rc) return rc;

    /* End condition? */
    if (end_time > 0 && end_time < model_time) return 1;

    /* Push data to SimBus. */
    if (sim->sequential_cosim) {
        /* The scalar final_val are already resolved in sim_step_models() so
        only marshal out the binary signals. */
        marshal(sim, MARSHAL_MODEL2ADAPTER_BINARY_ONLY);
    } else {
        marshal(sim, MARSHAL_MODEL2ADAPTER);
    }
    rc = adapter_model_ready(adapter, sim);
    if (rc) return rc;

    /* Otherwise, return 0 indicating that do_step was successful. */
    return 0;
}


void controller_run(SimulationSpec* sim)
{
    assert(sim);
    Controller* controller = __controller;
    if (controller == NULL) return;

    /* ModelRegister (etc). */
    controller_bus_ready(sim);

    /* ModelReady, ModelStart, do_step(). */
    while (true) {
        /* Check if stop requested. */
        if (controller->stop_request == true) {
            errno = ECANCELED;
            break;
        }
        int rc = controller_step(sim);
        if (rc != 0) break;
    }
}


/* Called from an interrupt. Indicate that controller_run() should exit. */
void controller_stop(void)
{
    Controller* controller = __controller;
    if (controller == NULL) return;

    controller->stop_request = true;
    if (controller->adapter) adapter_interrupt(controller->adapter);
}


void controller_dump_debug(void)
{
    Controller* controller = __controller;

    if (controller && controller->adapter) {
        adapter_dump_debug(controller->adapter, controller->simulation);
    }
}


void controller_exit(SimulationSpec* sim)
{
    Controller* controller = __controller;
    if (controller == NULL) return;
    if (sim == NULL) return;

    ModelInstanceSpec* _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        if (_instptr->model_desc) {
            if (_instptr->model_desc->vtable.destroy == NULL) goto exit_next;

            log_notice("Call symbol: %s ...", MODEL_DESTROY_FUNC_NAME);
            errno = 0;
            _instptr->model_desc->vtable.destroy(_instptr->model_desc);
            if (errno) log_error(MODEL_DESTROY_FUNC_NAME "() failed");
        }
    exit_next:
        /* Next instance? */
        _instptr++;
    }

    log_notice("Controller exit ...");
    if (controller->adapter) adapter_exit(controller->adapter, sim);

    /* No retreat, no surrender. */
    controller_destroy();
}
