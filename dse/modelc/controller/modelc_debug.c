// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/model.h>


/*
 *  The ModelC Debug API
 *  ====================
 */


/**
 *  modelc_step
 *
 *  Execute a simulation step with the provided step size for all model
 * functions of the given model instance.
 *
 *  Parameters
 *  ----------
 *  model_instance : ModelInstanceSpec (pointer to)
 *      The model instance, which holds references to the registered channels
 * and model functions. step_size : double The duration simulation step to be
 * performed (in seconds).
 *
 *  Returns
 *  -------
 *      0 : Success.
 *      +ve/-ve : Failure, inspect `errno` for the failing condition.
 */
int modelc_step(ModelInstanceSpec* model_instance, double step_size)
{
    ModelInstancePrivate* mip = model_instance->private;

    mip->adapter_model->stop_time = mip->adapter_model->model_time + step_size;
    double model_time = 0.0;
    return step_model(model_instance, &model_time);
}


/**
 *  modelc_destroy
 *
 *  Bypass the controller and call model_destroy() directly.
 *
 *  Parameters
 *  ----------
 *  model_instance : ModelInstanceSpec (pointer to)
 *      The model instance, which holds references to the registered channels
 * and model functions. step_size : double The duration simulation step to be
 * performed (in seconds).
 */
void modelc_destroy(ModelInstanceSpec* mi)
{
    if (mi == NULL) return;
    if (mi->model_desc == NULL) return;

    if (mi->model_desc->vtable.destroy == NULL) return;
    mi->model_desc->vtable.destroy(mi->model_desc);
    mi->model_desc->vtable.destroy = NULL;
}
