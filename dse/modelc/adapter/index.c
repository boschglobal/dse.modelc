// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/private.h>
#include <dse/modelc/adapter/uid_index.h>


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
    }
    if (channel->index.map) {
        free(channel->index.map);
        channel->index.map = NULL;
    }
    channel->index.count = 0;
    signal_value_uid_index_destroy(channel->index.uid2sv_lookup);
    channel->index.uid2sv_lookup = NULL;
}

void _generate_index(Channel* channel)
{
    _destroy_index(channel);

    channel->index.count = hashmap_number_keys(channel->signal_values);
    if (channel->index.count == 0) return;

    channel->index.names = hashmap_keys(&channel->signal_values);
    channel->index.map = _get_signal_value_map(
        channel, (const char**)channel->index.names, channel->index.count);
}

void _invalidate_index(Channel* channel)
{
    if (channel->index.names != NULL) {
        channel->index.hash_code++;  // Signal consumers of index change.
        _destroy_index(channel);
    }
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
    ChannelIndexItem* item = vector_find(&am->channels,
        &(ChannelIndexItem){ .name = channel_name, .ch = NULL }, 0, NULL);
    if (item) {
        assert(strcmp(item->ch->name, channel_name) == 0);
        return item->ch;
    }
    log_simbus("call: _get_channel() : %s", channel_name);
    log_error("Channel not initialised!");
    assert(0); /* Should not happen. */
    return NULL;
}


Channel* _get_channel_byindex(AdapterModel* am, uint32_t index)
{
    assert(index < vector_len(&am->channels));
    ChannelIndexItem* item = vector_at(&am->channels, index, NULL);
    return item->ch;
}


/*
Signal related internal API
---------------------------
*/
SignalValue* _find_signal_by_uid(Channel* channel, uint32_t uid)
{
    if (uid == 0) return NULL;
    SignalValue* indexed_signal =
        signal_value_uid_index_find(channel->index.uid2sv_lookup, uid);
    if (indexed_signal != NULL) return indexed_signal;

    /* Fallback, linear search (UID is updated by Simbus messages). */
    SignalValue* sv = NULL;
    for (unsigned int i = 0; i < channel->index.count; i++) {
        sv = channel->index.map[i].signal;
        if (sv->uid == uid) {
            signal_value_uid_index_insert(
                &channel->index.uid2sv_lookup, uid, sv);
            errno = 0;
            return sv;
        }
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
    /* PERFORMANCE: call _refresh_index() before calling this function from
       a loop to ensure the index is recreated before accessing elements.

       Calling _refresh_index() hits performance when inside the loop (i.e.
       here in this function).

        _refresh_index(channel);
        for (uint32_t i = 0; i < ch->index.count; i++) {

        }
    */
    return channel->index.map[index].signal;
}

SignalMap* _get_signal_value_map(
    Channel* channel, const char** signal_name, uint32_t signal_count)
{
    SignalMap* sm = calloc(signal_count, sizeof(SignalMap));
    for (uint32_t i = 0; i < signal_count; i++) {
        SignalValue* sv = _get_signal_value(channel, signal_name[i]);
        sm[i].name = signal_name[i];
        sm[i].signal = sv;
        /* Add to the lookup index. */
        if (sv->uid) {
            signal_value_uid_index_insert(
                &channel->index.uid2sv_lookup, sv->uid, sv);
        }
    }
    errno = 0;
    return sm;
}
