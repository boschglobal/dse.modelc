// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
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
    /* Add the Model Function to the lookup vector. */
    ModelInstancePrivate*  mip = model_instance->private;
    ControllerModel*       cm = mip->controller_model;
    ModelFunctionIndexItem item = { .name = model_function->name,
        .mf = model_function };
    vector_push(&cm->model_functions, &item);
    vector_sort(&cm->model_functions);

    return 0;
}


ModelFunction* controller_get_model_function(
    ModelInstanceSpec* model_instance, const char* model_function_name)
{
    assert(model_instance);
    ModelInstancePrivate* mip = model_instance->private;
    ControllerModel*      cm = mip->controller_model;

    ModelFunctionIndexItem* item = vector_find(&cm->model_functions,
        &(ModelFunctionIndexItem){ .name = model_function_name, .mf = NULL }, 0,
        NULL);
    if (item) return item->mf;
    return NULL;
}


const char* controller_get_signal_annotation(
    ModelFunctionChannel* mfc, const char* signal_name, const char* name)
{
    if (mfc == NULL) return NULL;
    assert(signal_name);
    assert(name);

    if (hashmap_number_keys(mfc->signal_annotation_map) == 0) return NULL;
    YamlNode* annotation =
        hashmap_get(&mfc->signal_annotation_map, signal_name);
    return annotation ? dse_yaml_get_scalar(annotation, name) : NULL;
}
