// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/private.h>


/*
Index related internal API
--------------------------
*/

void _destroy_index(Channel* channel)
{
    if (channel->index.names) {
        for (uint32_t _ = 0; _ < channel->index.count; _++)
            free(channel->index.names[_]);
        free(channel->index.names);
        channel->index.names = NULL;
        channel->index.count = 0;
    }
    if (channel->index.map) {
        free(channel->index.map);
        channel->index.map = NULL;
    }
}

void _generate_index(Channel* channel)
{
    _destroy_index(channel);

    channel->index.names = hashmap_keys(&channel->signal_values);
    channel->index.count = hashmap_number_keys(channel->signal_values);
    channel->index.map = _get_signal_value_map(
        channel, (const char**)channel->index.names, channel->index.count);
}

void _invalidate_index(Channel* channel)
{
    channel->index.hash_code++;  // Signal consumers of index change.
    _destroy_index(channel);
}

void _refresh_index(Channel* channel)
{
    if (channel->index.names == NULL) _generate_index(channel);
}


/*
Channel related internal API
----------------------------
*/

Channel* _get_channel(AdapterModel* am, const char* channel_name)
{
    Channel* ch = hashmap_get(&am->channels, channel_name);
    if (ch) {
        assert(strcmp(ch->name, channel_name) == 0);
        return ch;
    }
    log_simbus("call: _get_channel() : %s", channel_name);
    log_error("Channel not initialised!");
    assert(0); /* Should not happen. */
    return NULL;
}


Channel* _get_channel_byindex(AdapterModel* am, uint32_t index)
{
    assert(index < am->channels_length);
    const char* channel_name = am->channels_keys[index];
    return _get_channel(am, channel_name);
}


/*
Signal related internal API
---------------------------
*/

SignalValue* _find_signal_by_uid(Channel* channel, uint32_t uid)
{
    if (uid == 0) return NULL;

    for (unsigned int i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        if (sv->uid == uid) return sv;
    }
    return NULL;
}


SignalValue* _get_signal_value(Channel* channel, const char* signal_name)
{
    SignalValue* sv = hashmap_get(&channel->signal_values, signal_name);
    if (sv) return sv;

    /* Add a new SignalValue, assume dynamically provided name. */
    sv = calloc(1, sizeof(SignalValue));
    sv->name = strdup(signal_name);
    sv = hashmap_set(&channel->signal_values, signal_name, sv);
    assert(sv);
    _invalidate_index(channel);

    return sv;
}

SignalValue* _get_signal_value_byindex(Channel* channel, uint32_t index)
{
    _refresh_index(channel);
    assert(index < channel->index.count);
    return _get_signal_value(channel, channel->index.names[index]);
}

SignalMap* _get_signal_value_map(
    Channel* channel, const char** signal_name, uint32_t signal_count)
{
    SignalMap* sm;

    sm = calloc(signal_count, sizeof(SignalMap));
    for (uint32_t i = 0; i < signal_count; i++) {
        sm[i].name = signal_name[i];
        sm[i].signal = _get_signal_value(channel, signal_name[i]);
    }
    return sm;
}
