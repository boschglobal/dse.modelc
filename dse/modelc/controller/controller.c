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


#define UNUSED(x)   ((void)x)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)


typedef struct ControllerForeachContext {
    HashMapIterateFunc func;
    bool               continue_on_error;
    void*              data;
} ControllerForeachContext;


static int __controller_mfc_foreach(void* item, void* data)
{
    ControllerForeachContext*      ctx = data;
    ModelFunctionChannelIndexItem* mfc_item = item;
    int                            rc = ctx->func(mfc_item->mfc, ctx->data);
    if (rc && !ctx->continue_on_error) return rc;
    return 0;
}

static int __controller_mf_foreach(void* item, void* data)
{
    ControllerForeachContext* ctx = data;
    ModelFunctionIndexItem*   mf_item = item;
    int                       rc = ctx->func(mf_item->mf, ctx->data);
    if (rc && !ctx->continue_on_error) return rc;
    return 0;
}


DLL_PRIVATE Controller* controller_object_ref(SimulationSpec* sim)
{
    ModelInstancePrivate* mip = sim->instance_list->private;
    return mip->controller;
}


void controller_destroy(SimulationSpec* sim)
{
    ModelInstancePrivate* mip = sim->instance_list->private;
    Controller*           controller = mip->controller;
    if (controller == NULL) return;

    if (controller->adapter) adapter_destroy(controller->adapter);

    free(mip->controller);
    mip->controller = NULL;
}


int controller_init(Endpoint* endpoint, SimulationSpec* sim)
{
    ModelInstancePrivate* mip = sim->instance_list->private;
    assert(mip);
    assert(mip->controller == NULL);

    errno = 0;
    mip->controller = calloc(1, sizeof(Controller));
    if (mip->controller == NULL) {
        log_error("Controller malloc failed!");
        goto error_clean_up;
    }

    Controller* controller = mip->controller;
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
    controller_destroy(sim);
    return -1;
}


int controller_init_channel(ModelInstanceSpec* model_instance,
    const char* channel_name, const char** signal_name, uint32_t signal_count,
    ModelFunctionChannel* mfc)
{
    assert(model_instance);
    ModelInstancePrivate* mip = model_instance->private;
    AdapterModel*         am = mip->adapter_model;

    log_notice("Init Controller channel: %s", channel_name);
    adapter_init_channel(am, channel_name, signal_name, signal_count, mfc);
    if (mip->controller != NULL && mip->controller->simulation != NULL) {
        am->adapter->sequential_cosim =
            mip->controller->simulation->sequential_cosim;
    }
    mfc->scalar_sync.capable =
        am->adapter != NULL && am->adapter->vtable != NULL &&
        am->adapter->vtable->mode.flat_scalar && !am->adapter->sequential_cosim;

    return 0;
}


static int __marshal__adapter2model(void* _mfc, void* _spec)
{
    ModelFunctionChannel*  mfc = _mfc;
    ControllerMarshalSpec* spec = _spec;
    ModelInstancePrivate*  mip = spec->mi->private;
    AdapterModel*          am = mip->adapter_model;

    const bool flat_scalar = mfc->signal_transform == NULL &&
                             mfc->signal_value_binary == NULL &&
                             mfc->scalar_sync.enabled;
    if ((!flat_scalar || mfc->scalar_sync.simbus_indices == NULL ||
            mfc->scalar_sync.sync_count != mfc->signal_count) &&
        mfc->signal_map == NULL) {
        mfc->signal_map = adapter_get_signal_map(
            am, mfc->channel_name, mfc->signal_names, mfc->signal_count);
    }
    SignalMap* sm = mfc->signal_map;

    if (mfc->signal_value_double) {
        if (mfc->scalar_sync.enabled && mfc->signal_map != NULL &&
            (mfc->scalar_sync.simbus_indices == NULL ||
                mfc->scalar_sync.sync_count != mfc->signal_count)) {
            model_function_channel_build_flat_sync(mfc, sm);
        }
        controller_transform_to_model(mfc, sm, mip->lua_state);
    }
    if (spec->dir == MARSHAL_ADAPTER2MODEL_SCALAR_ONLY) return 0;
    if (mfc->signal_value_binary == NULL) return 0;

    const uint32_t count = mfc->signal_count;
    for (uint32_t si = 0; si < count; si++) {
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

    return 0;
}
static int __marshal__model2adapter(void* _mfc, void* _spec)
{
    ModelFunctionChannel*  mfc = _mfc;
    ControllerMarshalSpec* spec = _spec;
    ModelInstancePrivate*  mip = spec->mi->private;
    AdapterModel*          am = mip->adapter_model;

    const bool flat_scalar = mfc->signal_transform == NULL &&
                             mfc->signal_value_binary == NULL &&
                             mfc->scalar_sync.enabled;
    SignalMap* sm = mfc->signal_map;
    if ((!flat_scalar || mfc->scalar_sync.simbus_indices == NULL ||
            mfc->scalar_sync.sync_count != mfc->signal_count) &&
        UNLIKELY(sm == NULL)) {
        mfc->signal_map = adapter_get_signal_map(
            am, mfc->channel_name, mfc->signal_names, mfc->signal_count);
        sm = mfc->signal_map;
    }

    if (spec->dir == MARSHAL_MODEL2ADAPTER ||
        spec->dir == MARSHAL_MODEL2ADAPTER_SCALAR_ONLY) {
        if (mfc->signal_value_double) {
            if (mfc->scalar_sync.enabled && mfc->signal_map != NULL &&
                (mfc->scalar_sync.simbus_indices == NULL ||
                    mfc->scalar_sync.sync_count != mfc->signal_count)) {
                model_function_channel_build_flat_sync(mfc, sm);
            }
            controller_transform_from_model(mfc, sm, mip->lua_state);
        }
    }

    if (spec->dir == MARSHAL_MODEL2ADAPTER ||
        spec->dir == MARSHAL_MODEL2ADAPTER_BINARY_ONLY)
        return 0;
    if (mfc->signal_value_binary == NULL) return 0;

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
            &sm[si].signal->bin_buffer_size, mfc->signal_value_binary[si],
            mfc->signal_value_binary_size[si]);
        /* Indicate the binary object was consumed. */
        mfc->signal_value_binary_size[si] = 0;
    }
    return 0;
}
static int __marshal__model_function(void* _mf, void* _spec)
{
    ModelFunction*           mf = _mf;
    ControllerMarshalSpec*   spec = _spec;
    int                      rc = 0;
    ControllerForeachContext ctx = { 0 };

    switch (spec->dir) {
    case MARSHAL_ADAPTER2MODEL:
    case MARSHAL_ADAPTER2MODEL_SCALAR_ONLY:
        ctx.func = __marshal__adapter2model;
        break;
    case MARSHAL_MODEL2ADAPTER:
    case MARSHAL_MODEL2ADAPTER_SCALAR_ONLY:
    case MARSHAL_MODEL2ADAPTER_BINARY_ONLY:
        ctx.func = __marshal__model2adapter;
        break;
    default:
        return 0;
    }

    ctx.continue_on_error = false;
    ctx.data = spec;
    rc = vector_foreach(&mf->channels, __controller_mfc_foreach, &ctx);
    return rc;
}
static int __marshal__model(ControllerModel* cm, void* spec)
{
    ControllerForeachContext ctx = {
        .func = __marshal__model_function,
        .continue_on_error = false,
        .data = spec,
    };
    return vector_foreach(&cm->model_functions, __controller_mf_foreach, &ctx);
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
    assert(sim);
    ModelInstancePrivate* mip = sim->instance_list->private;
    assert(mip);
    Controller* controller = mip->controller;

    /* Explicitly start the endpoint (creates resources etc). */
    assert(controller->adapter);
    Adapter* adapter = controller->adapter;
    assert(adapter->endpoint);
    Endpoint* endpoint = adapter->endpoint;
    if (endpoint->start) {
        int32_t rc = endpoint->start(endpoint);
        if (rc != 0) {
            log_error("Endpoint start failed: %s", strerror(-rc));
            controller->stop_request = true;
            return;
        }
    }

    /* Connect with the bus. */
    adapter_connect(adapter, sim, 5);
    if (controller->stop_request) return;

    /* Register the signals with the bus. */
    adapter_register(adapter, sim);
}


int controller_step(SimulationSpec* sim)
{
    ModelInstancePrivate* mip = sim->instance_list->private;
    assert(mip);
    assert(mip->controller);
    Controller* controller = mip->controller;
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
    ModelInstancePrivate* mip = sim->instance_list->private;
    assert(mip);
    assert(mip->controller);
    Controller* controller = mip->controller;
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
    ModelInstancePrivate* mip = sim->instance_list->private;
    Controller* restrict controller = mip->controller;
    if (UNLIKELY(controller == NULL)) return;

    /* ModelRegister (etc). */
    controller_bus_ready(sim);

    /* ModelReady, ModelStart, do_step(). */
    volatile bool* restrict stop_flag = &controller->stop_request;
    while (true) {
        /* Check if stop requested. */
        if (UNLIKELY(*stop_flag == true)) {
            errno = ECANCELED;
            break;
        }
        /* Step. */
        if (UNLIKELY(controller_step(sim) != 0)) {
            break;
        }
    }
}


/* Called from an interrupt. Indicate that controller_run() should exit. */
void controller_stop(SimulationSpec* sim)
{
    ModelInstancePrivate* mip = sim->instance_list->private;
    Controller*           controller = mip->controller;
    if (controller == NULL) return;

    controller->stop_request = true;
    if (controller->adapter) adapter_interrupt(controller->adapter);
}


void controller_dump_debug(SimulationSpec* sim)
{
    ModelInstancePrivate* mip = sim->instance_list->private;
    Controller*           controller = mip->controller;

    if (controller && controller->adapter) {
        adapter_dump_debug(controller->adapter, controller->simulation);
    }
}


void controller_exit(SimulationSpec* sim)
{
    ModelInstancePrivate* mip = sim->instance_list->private;
    Controller*           controller = mip->controller;
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
    controller_destroy(sim);
}
