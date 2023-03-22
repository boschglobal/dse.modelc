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


/**
 *  The ModelC Debug API
 *  ====================
 *
 *  Example
 *  -------
 *  #include <dse/modelc/model.h>
 *
 *  void print_sv_names(SimulationSpec *sim, const char *name)
 *  {
 *      ModelInstanceSpec* mi = modelc_get_model_instance(sim, name);
 *      ModelChannelDesc *sv = modelc_get_model_vectors(mi);
 *      while (sv && sv->name) {
 *         printf("SV Name is: %s\n", sv->name);
 *         // Next item.
 *         sv++;
 *      }
 *      free(sv);
 *  }
 */


typedef struct channel_collection_data {
    ModelChannelDesc* collection;
    uint32_t          collection_max_index;
    uint32_t          current_collection_index;
    const char*       current_modelfunction_name;
} channel_collection_data;


static int _add_channel(void* _mfc, void* _channel_collection_data)
{
    ModelFunctionChannel*    mfc = _mfc;
    channel_collection_data* data = _channel_collection_data;

    if (data->collection_max_index <= data->current_collection_index) {
        log_error("New Index %d exceeds max index %d",
            data->current_collection_index, data->collection_max_index);
        return -1;
    }
    ModelChannelDesc* current_mcd =
        &(data->collection[data->current_collection_index]);

    current_mcd->name = mfc->channel_name;
    current_mcd->signal_names = mfc->signal_names;
    current_mcd->function_name = data->current_modelfunction_name;
    current_mcd->signal_count = mfc->signal_count;
    current_mcd->vector_double = mfc->signal_value_double;
    current_mcd->vector_binary = mfc->signal_value_binary;

    data->current_collection_index++;
    return 0;
}


static int _collect_channels(void* _mf, void* _channel_collection_data)
{
    ModelFunction*           mf = _mf;
    channel_collection_data* data = _channel_collection_data;
    data->current_modelfunction_name = mf->name;
    int rc = hashmap_iterator(&mf->channels, _add_channel, false, data);
    return rc;
}


static int _count_channels(void* _mf, void* _number)
{
    ModelFunction* mf = _mf;
    uint64_t*      cnt = _number;
    uint64_t       _cnt = hashmap_number_keys(mf->channels);
    *cnt += _cnt;
    return 0;
}


/**
 *  modelc_get_model_vectors
 *
 *  Collect and return information of all channels registered for the given
 * model instance.
 *
 *  Parameters
 *  ----------
 *  model_instance : ModelInstanceSpec (pointer to)
 *      The model instance, which holds references to the registered channels.
 *
 *  Returns
 *  -------
 *      ModelChannelDesc (pointer to list/array) : A list of ModelChannelDesc
 * objects where the list is terminated by a NULL element (i.e. desc->name ==
 * NULL). Caller to free.
 */
ModelChannelDesc* modelc_get_model_vectors(ModelInstanceSpec* model_instance)
{
    ModelInstancePrivate* mip = model_instance->private;
    uint64_t              channel_count = 0;
    HashMap* model_function_map = &mip->controller_model->model_functions;
    int      rc = hashmap_iterator(
             model_function_map, _count_channels, false, &channel_count);
    if (rc) {
        log_error("Iterating data structure failed");
        return NULL;
    }
    log_debug("Found %d channels in model instance", channel_count);
    ModelChannelDesc* channels =
        calloc(channel_count + 1, sizeof(ModelChannelDesc));
    channel_collection_data ccd = { channels, channel_count, 0, NULL };
    rc = hashmap_iterator(model_function_map, _collect_channels, false, &ccd);
    if (rc) {
        log_error("Iterating data structure failed");
        return NULL;
    }
    return channels;
}


/**
 *  modelc_get_model_instance
 *
 *  Find the ModelInstanceSpec object representing the model instance with the
 *  given name in the referenced SimulationSpec* object.
 *
 *  Parameters
 *  ----------
 *  sim : SimulationSpec (pointer to)
 *      Simulation specification (contains all Model Instance objects).
 *  name : const char*
 *      Name of the requested model instance.
 *
 *  Returns
 *  -------
 *      ModelInstanceSpec (pointer to) : First model instance, whose property
 * `name` is equal to the parameter `name`. NULL : No matching ModelInstanceSpec
 * object found.
 */
ModelInstanceSpec* modelc_get_model_instance(
    SimulationSpec* sim, const char* name)
{
    ModelInstanceSpec* _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        if (strcmp(_instptr->name, name) == 0) return _instptr;
        /* Next instance. */
        _instptr++;
    }
    return NULL;
}


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
