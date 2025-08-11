// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <dse/testing.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/model.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/logger.h>


#define UNUSED(x) ((void)x)


static Endpoint*   __endpoint_handle;
static Controller* __controller;


void controller_run(SimulationSpec* sim)
{
    UNUSED(sim);
    return;
}

void controller_bus_ready(SimulationSpec* sim)
{
    UNUSED(sim);
    return;
}

void marshal_model(ModelInstanceSpec* mi, ControllerMarshalDir dir)
{
    UNUSED(mi);
    UNUSED(dir);
    return;
}

int controller_step(SimulationSpec* sim)
{
    /* Set the model stop time in the adapter object. Simulates the reception
       of a MODEL_START message. */
    ModelInstanceSpec* _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        ModelInstancePrivate* mip = _instptr->private;
        AdapterModel*         am = mip->adapter_model;
        am->stop_time = am->model_time + sim->step_size;
        /* Next instance? */
        _instptr++;
    }

    /* Step the models. */
    int    rc = 0;
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
    UNUSED(sim);
    return 0;
}

void controller_stop(SimulationSpec* sim)
{
    UNUSED(sim);
    return;
}

void controller_dump_debug(SimulationSpec* sim)
{
    UNUSED(sim);
    return;
}

void controller_exit(SimulationSpec* sim)
{
    ModelInstanceSpec*    _instptr = sim->instance_list;
    ModelInstancePrivate* mip = sim->instance_list->private;
    while (_instptr && _instptr->name) {
        if (_instptr->model_desc == NULL) goto exit_next;
        if (_instptr->model_desc->vtable.destroy == NULL) goto exit_next;

        log_notice("Call symbol: %s ...", MODEL_DESTROY_FUNC_NAME);
        errno = 0;
        _instptr->model_desc->vtable.destroy(_instptr->model_desc);
        if (errno) log_error(MODEL_DESTROY_FUNC_NAME "() failed");

    exit_next:
        /* Next instance? */
        _instptr++;
    }

    if (mip->controller) hashmap_destroy(&(mip->controller->adapter->models));
}

int controller_init(Endpoint* endpoint, SimulationSpec* sim)
{
    UNUSED(endpoint);
    ModelInstancePrivate* mip = sim->instance_list->private;
    mip->controller = __controller;

    hashmap_init(&(mip->controller->adapter->models));

    return 0;
}

int controller_init_channel(ModelInstanceSpec* model_instance,
    const char* channel_name, const char** signal_name, uint32_t signal_count)
{
    UNUSED(model_instance);
    UNUSED(channel_name);
    UNUSED(signal_name);
    UNUSED(signal_count);

    return 0;
}

Endpoint* endpoint_create(const char* transport, const char* uri, uint32_t uid,
    bool bus_mode, double timeout)
{
    UNUSED(transport);
    UNUSED(uri);
    UNUSED(uid);
    UNUSED(bus_mode);
    UNUSED(timeout);

    return __endpoint_handle;
}

Controller* controller_object_ref(void)
{
    return __controller;
}

void adapter_destroy_adapter_model(AdapterModel* am)
{
    hashmap_destroy(&am->channels);
    free(am);
}

void stub_setup_objects(Controller* c, Endpoint* e)
{
    __controller = c;
    __endpoint_handle = e;
}

void stub_release_objects(Controller* c, Endpoint* e)
{
    UNUSED(c);
    UNUSED(e);

    __controller = NULL;
    __endpoint_handle = NULL;
}
