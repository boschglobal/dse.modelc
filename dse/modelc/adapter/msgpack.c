// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <msgpack.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/adapter.h>


void sv_delta_to_msgpack(
    Channel* channel, msgpack_packer* pk, const char* log_prefix)
{
    /* First(root) Object, array, 2 elements. */
    msgpack_pack_array(pk, 2);
    uint32_t changed_signal_count = 0;
    for (uint32_t i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        if (sv->uid == 0) continue;
        if ((sv->val != sv->final_val) || (sv->bin && sv->bin_size)) {
            changed_signal_count++;
        }
    }
    /* 1st Object in root Array, list of UID's. */
    msgpack_pack_array(pk, changed_signal_count);
    for (uint32_t i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        if (sv->uid == 0) continue;
        if ((sv->val != sv->final_val) || (sv->bin && sv->bin_size)) {
            msgpack_pack_uint32(pk, sv->uid);
        }
    }
    /* 2st Object in root Array, list of Values. */
    msgpack_pack_array(pk, changed_signal_count);
    for (uint32_t i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        if (sv->uid == 0) continue;
        if (sv->bin && sv->bin_size) {
            msgpack_pack_bin_with_body(pk, sv->bin, sv->bin_size);
            log_simbus("    %s: %u = <binary> (len=%u) [name=%s]", log_prefix,
                sv->uid, sv->bin_size, sv->name);
            /* Indicate the binary object was consumed. */
            sv->bin_size = 0;
        } else if (sv->val != sv->final_val) {
            msgpack_pack_double(pk, sv->final_val);
            log_simbus("    %s: %u = %f [name=%s]", sv->uid, log_prefix,
                sv->final_val, sv->name);
        }
    }
}

void sv_delta_to_msgpack_binonly(
    Channel* channel, msgpack_packer* pk, const char* log_prefix)
{
    /* First(root) Object, array, 2 elements. */
    msgpack_pack_array(pk, 2);
    uint32_t changed_signal_count = 0;
    for (uint32_t i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        if (sv->uid == 0) continue;
        if (sv->bin && sv->bin_size) {
            changed_signal_count++;
        }
    }
    /* 1st Object in root Array, list of UID's. */
    msgpack_pack_array(pk, changed_signal_count);
    for (uint32_t i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        if (sv->uid == 0) continue;
        if (sv->bin && sv->bin_size) {
            msgpack_pack_uint32(pk, sv->uid);
        }
    }
    /* 2st Object in root Array, list of Values. */
    msgpack_pack_array(pk, changed_signal_count);
    for (uint32_t i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        if (sv->uid == 0) continue;
        if (sv->bin && sv->bin_size) {
            msgpack_pack_bin_with_body(pk, sv->bin, sv->bin_size);
            log_simbus("    %s: %u = <binary> (len=%u) [name=%s]", log_prefix,
                sv->uid, sv->bin_size, sv->name);
            /* Indicate the binary object was consumed. */
            sv->bin_size = 0;
        }
    }
}
