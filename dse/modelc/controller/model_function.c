// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/controller/model_private.h>


int controller_register_model_function(
    ModelInstanceSpec* model_instance, ModelFunction* model_function)
{
    assert(model_instance);
    assert(model_function);

    void* handle;

    /* Check if the Model Function is already registered. */
    handle =
        controller_get_model_function(model_instance, model_function->name);
    if (handle) {
        errno = EEXIST;
        log_error("ModelFunction already registered with Controller!");
        return -1;
    }
    /* Add the Model Function to the hashmap. */
    ModelInstancePrivate* mip = model_instance->private;
    ControllerModel*      cm = mip->controller_model;
    HashMap*              mf_map = &cm->model_functions;
    handle = hashmap_set(mf_map, model_function->name, model_function);
    if (handle == NULL) {
        if (errno == 0) errno = EINVAL;
        log_error("ModelFunction failed to register with Controller!");
        return -1;
    }

    return 0;
}


ModelFunction* controller_get_model_function(
    ModelInstanceSpec* model_instance, const char* model_function_name)
{
    assert(model_instance);
    ModelInstancePrivate* mip = model_instance->private;
    ControllerModel*      cm = mip->controller_model;
    HashMap*              mf_map = &cm->model_functions;

    ModelFunction* mf = hashmap_get(mf_map, model_function_name);
    if (mf) return mf;
    return NULL;
}
