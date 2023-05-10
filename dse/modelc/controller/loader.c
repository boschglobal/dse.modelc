// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <dlfcn.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/controller/model_private.h>


extern Controller* controller_object_ref(void);

static ModelSetupHandler __model_setup_func = NULL;
static ModelExitHandler  __model_exit_func = NULL;

extern int __model_gw_setup__(ModelInstanceSpec* mi);
extern int __model_gw_exit__(ModelInstanceSpec* mi);


int controller_load_model(ModelInstanceSpec* model_instance)
{
    assert(model_instance);
    ModelInstancePrivate* mip = model_instance->private;
    ControllerModel*      controller_model = mip->controller_model;
    assert(controller_model);
    ModelSetupHandler model_setup_func = NULL;
    ModelExitHandler  model_exit_func = NULL;
    const char* dynlib_filename = model_instance->model_definition.full_path;

    errno = 0;

    if (dynlib_filename) {
        log_notice("Loading dynamic model: %s ...", dynlib_filename);
        void* handle = dlopen(dynlib_filename, RTLD_NOW | RTLD_LOCAL);
        if (handle == NULL) {
            log_notice("ERROR: dlopen call: %s", dlerror());
            goto error_dl;
        }
        log_notice("Loading symbol: %s ...", MODEL_SETUP_FUNC_STR);
        model_setup_func = dlsym(handle, MODEL_SETUP_FUNC_STR);
        if (model_setup_func == NULL) {
            log_notice("ERROR: dlsym call: %s", dlerror());
            goto error_dl;
        }
        log_notice("Loading optional symbol: %s ...", MODEL_EXIT_FUNC_STR);
        model_exit_func = dlsym(handle, MODEL_EXIT_FUNC_STR);
        if (model_exit_func) {
            log_debug("... symbol loaded");
        }
    } else {
        if (dse_yaml_find_node(
                model_instance->model_definition.doc, "spec/runtime/gateway")) {
            log_notice("Using gateway symbols: ...");
            model_setup_func = __model_gw_setup__;
            model_exit_func = __model_gw_exit__;
        } else {
            log_notice("Using linked/registered symbols: ...");
            model_setup_func = __model_setup_func;
            model_exit_func = __model_exit_func;
        }
    }

    controller_model->model_setup_func = model_setup_func;
    controller_model->model_exit_func = model_exit_func;
    return 0;

error_dl:
    if (errno == 0) errno = EINVAL;
    log_error("Failed to load dynamic model!");
    return errno;
}


int controller_load_models(SimulationSpec* sim)
{
    assert(sim);
    Controller* controller = controller_object_ref();
    int         rc = 0;

    controller->simulation = sim;

    Adapter* adapter = controller->adapter;
    assert(adapter);

    ModelInstanceSpec* _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        ModelInstancePrivate* mip = _instptr->private;
        AdapterModel*         am = mip->adapter_model;
        ControllerModel*      cm = mip->controller_model;

        am->adapter = adapter;
        am->model_uid = _instptr->uid;
        /* Set the UID based lookup for Adapter Model. */
        char hash_key[UID_KEY_LEN];
        snprintf(hash_key, UID_KEY_LEN - 1, "%d", _instptr->uid);
        hashmap_set(&adapter->models, hash_key, am);
        /* Load the Model. */
        errno = 0;
        rc = controller_load_model(_instptr);
        if (rc) {
            if (errno == 0) errno = EINVAL;
            log_error("controller_load_model() failed!");
            break;
        }
        /* Call model_setup(), inversion of control. */
        log_notice("Call symbol: %s ...", MODEL_SETUP_FUNC_STR);
        if (cm->model_setup_func == NULL) {
            rc = errno = EINVAL;
            log_error("model_setup_func() not loaded!");
            break;
        }
        errno = 0;
        rc = cm->model_setup_func(_instptr);
        if (rc) {
            if (errno == 0) errno = EINVAL;
            log_error("model_setup_func() failed!");
            break;
        }
        /* Next instance? */
        _instptr++;
    }

    return rc;
}
