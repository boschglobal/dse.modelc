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


#define HASH_UID_KEY_LEN (10 + 1)


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
    vector_reset(&channel->index.uid2sv_lookup);
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


int adapter_uid2sv_compar(const void* a, const void* b)
{
    const SignalValueIndexItem* x = a;
    const SignalValueIndexItem* y = b;
    if (x->uid < y->uid) return -1;
    if (x->uid > y->uid) return 1;
    return 0;
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
    SignalValueIndexItem* idx = vector_find(&channel->index.uid2sv_lookup,
        &(SignalValueIndexItem){ .uid = uid, .sv = NULL }, 0, NULL);
    if (idx != NULL) {
        assert(idx->sv);
        return idx->sv;
    }

    /* Fallback, linear search (UID is updated by Simbus messages). */
    SignalValue* sv = NULL;
    for (unsigned int i = 0; i < channel->index.count; i++) {
        sv = channel->index.map[i].signal;
        if (sv->uid == uid) {
            vector_push(&channel->index.uid2sv_lookup,
                &(SignalValueIndexItem){ .uid = uid, .sv = sv });
            vector_sort(&channel->index.uid2sv_lookup);
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
    Vector     uid2sv_working;
    uid2sv_working =
        vector_make(sizeof(SignalValueIndexItem), 0, adapter_uid2sv_compar);
    for (uint32_t i = 0; i < signal_count; i++) {
        SignalValue* sv = _get_signal_value(channel, signal_name[i]);
        sm[i].name = signal_name[i];
        sm[i].signal = sv;
        /* Add to the lookup index. */
        if (sv->uid) {
            SignalValueIndexItem idx = { .uid = sv->uid, .sv = sv };
            if (vector_find(&channel->index.uid2sv_lookup, &idx, 0, NULL) ==
                NULL) {
                vector_push(&uid2sv_working, &idx);
            }
        }
    }
    /* Merge new index items into the lookup vector. */
    uint32_t last_uid = 0;
    vector_sort(&uid2sv_working);
    for (size_t i = 0; i < vector_len(&uid2sv_working); i++) {
        SignalValueIndexItem idx;
        if (vector_at(&uid2sv_working, i, &idx)) {
            if (last_uid == idx.uid) continue;  // Duplicate.
            vector_push(&channel->index.uid2sv_lookup, &idx);
            last_uid = idx.uid;
        }
    }
    vector_reset(&uid2sv_working);
    /* Sort the lookup vector. */
    vector_sort(&channel->index.uid2sv_lookup);
    return sm;
}
