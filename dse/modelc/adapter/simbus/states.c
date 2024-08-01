// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <dse/modelc/adapter/simbus/simbus_private.h>
#include <dse/modelc/adapter/private.h>


#define UNUSED(x) ((void)x)


extern bool __simbus_exit_run_loop__;


bool simbus_network_ready(AdapterModel* am)
{
    for (uint32_t i = 0; i < am->channels_length; i++) {
        Channel* ch = _get_channel_byindex(am, i);
        if (ch->expected_model_count != set_length(ch->model_register_set))
            return false;
    }
    return true;
}


bool simbus_models_ready(AdapterModel* am)
{
    for (uint32_t i = 0; i < am->channels_length; i++) {
        Channel* ch = _get_channel_byindex(am, i);
        if (ch->expected_model_count != set_length(ch->model_ready_set))
            return false;
    }
    return true;
}


void simbus_models_to_start(AdapterModel* am)
{
    for (uint32_t i = 0; i < am->channels_length; i++) {
        Channel* ch = _get_channel_byindex(am, i);
        set_clear(ch->model_ready_set);
    }
}


void simbus_model_at_register(
    AdapterModel* am, Channel* channel, uint32_t model_uid)
{
    UNUSED(am);

    set_add_uint32(channel->model_register_set, model_uid);
}


void simbus_model_at_ready(
    AdapterModel* am, Channel* channel, uint32_t model_uid)
{
    UNUSED(am);

    set_add_uint32(channel->model_ready_set, model_uid);
}


void simbus_model_at_exit(
    AdapterModel* am, Channel* channel, uint32_t model_uid)
{
    set_remove_uint32(channel->model_register_set, model_uid);
    set_remove_uint32(channel->model_ready_set, model_uid);

    /* Exit the run loop? */
    for (uint32_t i = 0; i < am->channels_length; i++) {
        Channel* ch = _get_channel_byindex(am, i);
        if (set_length(ch->model_register_set)) return; /* Not 0 so no exit. */
    }
    __simbus_exit_run_loop__ = true;
}
