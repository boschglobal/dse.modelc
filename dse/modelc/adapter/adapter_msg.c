// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <dse/logger.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/message.h>
#include <dse/modelc/adapter/timer.h>
#include <dse/modelc/adapter/private.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/runtime.h>


#define UNUSED(x) ((void)x)


typedef struct notify_spec_t {
    Adapter* adapter;
    notify(NotifyMessage_table_t) message;
    flatcc_builder_t* builder;
    struct timespec   notifyrecv_ts;
    AdapterModel*     last_am;
} notify_spec_t;


static int32_t get_token(void)
{
    static int32_t _token = 0;
    return ++_token;
}


static int notify_encode_sv(void* value, void* data)
{
    AdapterModel*     am = value;
    notify_spec_t*    notify_data = data;
    flatcc_builder_t* B = notify_data->builder;
    assert(B);

    log_simbus("Notify/ModelReady --> [...]");
    log_simbus("    model_time=%f", am->model_time);

    /* Process scalar channels directly to FBS vectors. */
    for (uint32_t ch_idx = 0; ch_idx < am->channels_length; ch_idx++) {
        Channel* ch = _get_channel_byindex(am, ch_idx);
        assert(ch);
        _refresh_index(ch);
        log_simbus("  SignalVector --> [%s:%u]", ch->name, am->model_uid);

        notify(SignalVector_start(B));
        notify(SignalVector_name_add(
            B, flatbuffers_string_create_str(B, ch->name)));
        notify(SignalVector_model_uid_add(B, am->model_uid));

        /* Signal Vector. */
        size_t binary_signal_count = 0;
        notify(SignalVector_signal_start(B));
        for (uint32_t idx = 0; idx < ch->index.count; idx++) {
            SignalValue* sv = ch->index.map[idx].signal;
            if ((sv->val != sv->final_val) && sv->uid) {
                notify(
                    SignalVector_signal_push_create(B, sv->uid, sv->final_val));
                log_simbus("    SignalWrite: %u = %f [name=%s]", sv->uid,
                    sv->final_val, sv->name);
            }
            if (sv->bin && sv->bin_size && sv->uid) {
                /* Indicate that binary signals are present. */
                binary_signal_count++;
            }
        }
        notify(SignalVector_signal_add(B, notify(SignalVector_signal_end(B))));

        /* Binary Vector. */
        if (binary_signal_count) {
            notify(SignalVector_binary_signal_start(B));
            for (uint32_t idx = 0; idx < ch->index.count; idx++) {
                SignalValue* sv = ch->index.map[idx].signal;
                if (sv->bin && sv->bin_size && sv->uid) {
                    flatbuffers_uint8_vec_ref_t data =
                        flatbuffers_uint8_vec_create(
                            B, (uint8_t*)sv->bin, sv->bin_size);
                    notify(SignalVector_binary_signal_push_create(
                        B, sv->uid, data));
                    log_simbus(
                        "    SignalWrite: %u = <binary> (len=%u) [name=%s]",
                        sv->uid, sv->bin_size, sv->name);
                    /* Indicate the binary object was consumed. */
                    sv->bin_size = 0;
                }
            }
            notify(SignalVector_binary_signal_add(
                B, notify(SignalVector_binary_signal_end(B))));
        }

        notify(SignalVector_vec_push(B, notify(SignalVector_end(B))));
    }

    return 0;
}

static int notify_encode_model(void* value, void* data)
{
    AdapterModel*     am = value;
    notify_spec_t*    notify_data = data;
    flatcc_builder_t* B = notify_data->builder;
    assert(B);
    flatbuffers_uint32_vec_push(B, &am->model_uid);
    notify_data->last_am = am;

    return 0;
}


/*
Adapter Message Functions
-------------------------
*/

static int adapter_msg_connect(
    AdapterModel* am, SimulationSpec* sim, int retry_count)
{
    Adapter*          adapter = am->adapter;
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* B = &(v->builder);

    for (uint32_t channel_index = 0; channel_index < am->channels_length;
        channel_index++) {
        Channel* ch = _get_channel_byindex(am, channel_index);

        int rc = 0;
        for (int i = 0; i < retry_count; i++) {
            int32_t token = get_token();

            /* ModelRegister (without 'create') */
            log_simbus("ModelRegister --> [%s:%u]", ch->name, am->model_uid);
            log_simbus("    step_size=%f", sim->step_size);
            log_simbus("    model_uid=%d", am->model_uid);
            log_simbus("    notify_uid=%d", sim->uid);
            log_simbus("    token=%d", token);

            flatcc_builder_reset(B);

            /* ModelRegister message. */
            notify(ModelRegister_start(B));
            notify(ModelRegister_step_size_add)(B, sim->step_size);
            notify(ModelRegister_model_uid_add)(B, am->model_uid);
            notify(ModelRegister_notify_uid_add)(B, sim->uid);
            notify(ModelRegister_ref_t) model_register =
                notify(ModelRegister_end(B));

            /* UID vector. */
            flatbuffers_uint32_vec_start(B);
            flatbuffers_uint32_vec_push(B, &am->model_uid);
            flatbuffers_uint32_vec_ref_t model_uids =
                flatbuffers_uint32_vec_end(B);

            /* NotifyMessage container message.*/
            notify(NotifyMessage_start(B));
            notify(NotifyMessage_token_add(B, token));
            notify(NotifyMessage_model_uid_add(B, model_uids));
            notify(NotifyMessage_channel_name_add(
                B, flatbuffers_string_create_str(B, ch->name)));
            notify(NotifyMessage_model_register_add(B, model_register));
            notify(NotifyMessage_ref_t) message = notify(NotifyMessage_end(B));

            rc = send_notify_message_wait_ack(
                adapter, am->model_uid, message, token);
            if (rc == 0) break;
            if (adapter->stop_request) break;
            log_simbus("adapter_connect: retry (rc=%d)", rc);
        }
        if (rc) log_error("ModelRegister on [%s] failed!", ch->name);
    }

    return 0;
}


static int adapter_msg_register(AdapterModel* am)
{
    Adapter*          adapter = am->adapter;
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* B = &(v->builder);

    /* SignalIndex on all channels. */
    for (uint32_t channel_index = 0; channel_index < am->channels_length;
        channel_index++) {
        Channel* ch = _get_channel_byindex(am, channel_index);

        _refresh_index(ch);
        uint32_t signal_list_length = ch->index.count;
        int32_t  token = get_token();

        /* SignalIndex with SignalLookup. */
        log_simbus("SignalIndex --> [%s:%u]", ch->name, am->model_uid);
        log_simbus("    model_uid=%d", am->model_uid);
        log_simbus("    token=%d", token);

        flatcc_builder_reset(B);

        /* SignalLookups, encode into a vector. */
        notify(SignalLookup_ref_t)* signal_lookup_list =
            calloc(signal_list_length, sizeof(notify(SignalLookup_ref_t)));
        for (uint32_t i = 0; i < signal_list_length; i++) {
            SignalValue* sv = _get_signal_value_byindex(ch, i);

            flatbuffers_string_ref_t signal_name;
            signal_name = flatbuffers_string_create_str(B, sv->name);
            notify(SignalLookup_start(B));
            notify(SignalLookup_name_add(B, signal_name));
            const char* mime_type = controller_get_signal_annotation(
                ch->mfc, sv->name, "mime_type");
            if (mime_type != NULL) {
                flatbuffers_string_ref_t _ref;
                _ref = flatbuffers_string_create_str(B, mime_type);
                notify(SignalLookup_mime_type_add(B, _ref));
            }
            signal_lookup_list[i] = notify(SignalLookup_end(B));
            log_simbus("    SignalLookup: %s [UID=%u] [mime_type=%s]", sv->name,
                sv->uid, mime_type ? mime_type : "<none>");
        }
        notify(SignalLookup_vec_ref_t) signal_lookup_vector;
        notify(SignalLookup_vec_start(B));
        for (uint32_t i = 0; i < signal_list_length; i++)
            notify(SignalLookup_vec_push(B, signal_lookup_list[i]));
        signal_lookup_vector = notify(SignalLookup_vec_end(B));

        /* SignalIndex message. */
        notify(SignalIndex_start(B));
        notify(SignalIndex_indexes_add(B, signal_lookup_vector));
        notify(SignalIndex_ref_t) signal_index = notify(SignalIndex_end(B));

        /* UID vector. */
        flatbuffers_uint32_vec_start(B);
        flatbuffers_uint32_vec_push(B, &am->model_uid);
        flatbuffers_uint32_vec_ref_t model_uids = flatbuffers_uint32_vec_end(B);

        /* NotifyMessage container message. */
        notify(NotifyMessage_start(B));
        notify(NotifyMessage_token_add(B, token));
        notify(NotifyMessage_model_uid_add(B, model_uids));
        notify(NotifyMessage_channel_name_add(
            B, flatbuffers_string_create_str(B, ch->name)));
        notify(NotifyMessage_signal_index_add(B, signal_index));
        notify(NotifyMessage_ref_t) message = notify(NotifyMessage_end(B));

        send_notify_message(adapter, am->model_uid, message);
        free(signal_lookup_list);

        /* Wait on SignalIndex. */
        log_debug("adapter_register: wait on SignalIndex ...");
        {
            bool found = false;
            wait_message(adapter, NULL, token, &found);
            if (!found) {
                log_fatal("No SignalIndex received from SimBus!!!");
            }
        }
    }

    return 0;
}


static int adapter_msg_model_ready(Adapter* adapter)
{
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* B = &(v->builder);
    notify_spec_t     notify_data = {
            .adapter = adapter,
            .builder = B,
    };
    flatcc_builder_reset(B);

    /* SignalVector vector. */
    notify(SignalVector_vec_start(B));
    hashmap_iterator(&adapter->models, notify_encode_sv, true, &notify_data);
    notify(SignalVector_vec_ref_t) signals = notify(SignalVector_vec_end(B));

    /* Notify model_uid vector. */
    flatbuffers_uint32_vec_start(B);
    hashmap_iterator(&adapter->models, notify_encode_model, true, &notify_data);
    flatbuffers_uint32_vec_ref_t model_uids = flatbuffers_uint32_vec_end(B);
    AdapterModel*                am = notify_data.last_am;
    if (am == NULL) return 0;

    /* NotifyMessage message. */
    notify(NotifyMessage_start(B));
    notify(NotifyMessage_signals_add(B, signals));
    notify(NotifyMessage_model_uid_add(B, model_uids));
    notify(NotifyMessage_model_time_add(B, am->model_time));
    notify(NotifyMessage_bench_model_time_ns_add(B, am->bench_steptime_ns));
    notify(NotifyMessage_bench_notify_time_ns_add(B,
        get_elapsedtime_ns(am->bench_notifyrecv_ts) - am->bench_steptime_ns));
    notify(NotifyMessage_ref_t) message = notify(NotifyMessage_end(B));
    send_notify_message(adapter, 0, message);

    return 0;
}


static int adapter_msg_model_start(Adapter* adapter)
{
    /* Wait on Notify. Notify will contain all channel/signal values.
     * Indicated by parameters channel_name and message_type being NULL/0. */
    log_debug("adapter_ready: wait on Notify ...");
    bool found = false;
    int  rc = 0;

    rc = wait_message(adapter, NULL, 0, &found);
    if (rc != 0) {
        log_error("wait_message returned %d", rc);

        /* Handle specific error conditions. */
        if (rc == ETIME) return rc; /* TIMEOUT */
    }

    return 0;
}


static int adapter_msg_exit(AdapterModel* am)
{
    Adapter*          adapter = am->adapter;
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* B = &(v->builder);

    for (uint32_t channel_index = 0; channel_index < am->channels_length;
        channel_index++) {
        Channel* ch = _get_channel_byindex(am, channel_index);
        log_simbus("ModelExit --> [%s]", ch->name);

        flatcc_builder_reset(B);

        /* ModelExit message. */
        notify(ModelExit_start(B));
        notify(ModelExit_ref_t) model_exit = notify(ModelExit_end(B));

        /* UID vector. */
        flatbuffers_uint32_vec_start(B);
        flatbuffers_uint32_vec_push(B, &am->model_uid);
        flatbuffers_uint32_vec_ref_t model_uids = flatbuffers_uint32_vec_end(B);

        /* NotifyMessage container message.*/
        notify(NotifyMessage_start(B));
        notify(NotifyMessage_model_uid_add(B, model_uids));
        notify(NotifyMessage_channel_name_add(
            B, flatbuffers_string_create_str(B, ch->name)));
        notify(NotifyMessage_model_exit_add(B, model_exit));
        notify(NotifyMessage_ref_t) message = notify(NotifyMessage_end(B));

        send_notify_message(adapter, am->model_uid, message);
    }

    return 0;
}


static int notify_model(void* value, void* data)
{
    AdapterModel*  am = value;
    notify_spec_t* notify_data = data;
    am->bench_notifyrecv_ts = notify_data->notifyrecv_ts;
    notify(NotifyMessage_table_t) message = notify_data->message;
    log_simbus("Notify/ModelStart <-- [%u]", am->model_uid);
    am->model_time = notify(NotifyMessage_model_time(message));
    am->stop_time = notify(NotifyMessage_schedule_time(message));
    if (am->stop_time <= am->model_time) {
        // For now this is a fatal condition as it likely means the
        // SimBus is operating with 0 stepsize (i.e. misconfiguration).
        log_fatal("stop_time is NOT greater than model_time! Is step_size 0?");
    }
    log_simbus("    model_uid=%u", am->model_uid);
    log_simbus("    model_time=%f", am->model_time);
    log_simbus("    stop_time=%f", am->stop_time);

    /* Handle embedded SignalVector tables. */
    notify(SignalVector_vec_t) vector = notify(NotifyMessage_signals(message));
    size_t vector_len = notify(SignalVector_vec_len(vector));
    for (uint32_t _vi = 0; _vi < vector_len; _vi++) {
        /* Check the Lookup data is complete. */
        notify(SignalVector_table_t) signal_vector =
            notify(SignalVector_vec_at(vector, _vi));
        if (!notify(SignalVector_name_is_present(signal_vector))) {
            log_simbus("WARNING: signal vector name not provided!");
            continue;
        }
        const char* channel_name = notify(SignalVector_name(signal_vector));
        Channel*    channel = hashmap_get(&am->channels, channel_name);
        if (channel == NULL) continue;
        log_simbus("  SignalVector <-- [%s]", channel->name);

        /* Process vector encoded binary signal data. */
        notify(BinarySignal_vec_t) binary_signal_vec =
            notify(SignalVector_binary_signal(signal_vector));
        size_t binary_signal_vec_len =
            notify(BinarySignal_vec_len)(binary_signal_vec);
        for (size_t i = 0; i < binary_signal_vec_len; i++) {
            notify(BinarySignal_table_t) binary_signal =
                notify(BinarySignal_vec_at(binary_signal_vec, i));
            uint32_t _uid = notify(BinarySignal_uid(binary_signal));
            flatbuffers_uint8_vec_t data_vec =
                notify(BinarySignal_data(binary_signal));
            size_t data_vec_len = flatbuffers_uint8_vec_len(data_vec);

            SignalValue* sv = _find_signal_by_uid(channel, _uid);
            if ((sv == NULL) && _uid) {
                log_simbus("WARNING: signal with uid (%u) not found!", _uid);
                continue;
            }
            if (data_vec != NULL && data_vec_len) {
                dse_buffer_append(&sv->bin, &sv->bin_size, &sv->bin_buffer_size,
                    data_vec, data_vec_len);
                log_simbus("    SignalValue: %u = <binary> (len=%u) [name=%s]",
                    _uid, sv->bin_size, sv->name);
            }
        }

        /* Process vector encoded signal data. */
        notify(Signal_vec_t) signal_vec =
            notify(SignalVector_signal(signal_vector));
        size_t signal_length = notify(Signal_vec_len)(signal_vec);
        for (size_t i = 0; i < signal_length; i++) {
            notify(Signal_struct_t) signal =
                notify(Signal_vec_at(signal_vec, i));
            uint32_t _uid = signal->uid;
            double   _value = signal->value;

            SignalValue* sv = _find_signal_by_uid(channel, _uid);
            if ((sv == NULL) && _uid) {
                log_simbus("WARNING: signal with uid (%u) not found!", _uid);
                continue;
            }
            sv->val = _value;
            /* Reset final_val (changes will trigger SignalWrite) */
            sv->final_val = _value;
            log_simbus(
                "    SignalValue: %u = %f [name=%s]", _uid, sv->val, sv->name);
        }
    }

    return 0;
}

static void handle_notify_message(
    Adapter* adapter, notify(NotifyMessage_table_t) notify_message)
{
    /* Handle embedded SignalIndex message/table. */
    if (notify(NotifyMessage_signal_index_is_present(notify_message))) {
        /* NotifyMessage container message.*/
        if (!notify(NotifyMessage_channel_name_is_present(notify_message)))
            assert(0);
        const char* ch_name =
            notify(NotifyMessage_channel_name(notify_message));
        flatbuffers_uint32_vec_t vector =
            notify(NotifyMessage_model_uid(notify_message));
        size_t vector_len = flatbuffers_uint32_vec_len(vector);
        assert(vector_len == 1);
        uint32_t model_uid = flatbuffers_uint32_vec_at(vector, 0);

        /* Locate the channel objects. */
        char hash_key[UID_KEY_LEN];
        snprintf(hash_key, UID_KEY_LEN - 1, "%d", model_uid);
        AdapterModel* am = hashmap_get(&adapter->models, hash_key);
        if (am == NULL) return; /* Discard, not for this model. */
        Channel* channel = hashmap_get(&am->channels, ch_name);
        assert(channel);

        log_simbus("SignalIndex <-- [%s:%u]", channel->name, model_uid);
        log_simbus("    model_uid=%d", model_uid);

        /* SignalIndex table - extract. */
        notify(SignalIndex_table_t) t =
            notify(NotifyMessage_signal_index(notify_message));
        if (!notify(SignalIndex_indexes_is_present(t))) {
            log_simbus("WARNING: no indexes in SignalIndex message!");
        }
        notify(SignalLookup_vec_t) v = notify(SignalIndex_indexes(t));
        size_t v_len = notify(SignalLookup_vec_len(v));

        /* Update the indexes. */
        for (uint32_t _vi = 0; _vi < v_len; _vi++) {
            /* Check the Lookup data is complete. */
            notify(SignalLookup_table_t) signal_lookup =
                notify(SignalLookup_vec_at(v, _vi));
            if (!notify(SignalLookup_name_is_present(signal_lookup))) continue;
            const char* signal_name = notify(SignalLookup_name(signal_lookup));
            uint32_t    signal_uid =
                notify(SignalLookup_signal_uid(signal_lookup));
            log_simbus(
                "    SignalLookup: %s [UID=%u]", signal_name, signal_uid);
            if (signal_uid == 0) continue;
            /* Update the Adapter signal_value array. */
            SignalValue* sv = hashmap_get(&channel->signal_values, signal_name);
            if (sv) {
                sv->uid = signal_uid;
                /* Add to the lookup index. */
                hashmap_set_by_hash32(
                    &channel->index.uid2sv_lookup, signal_uid, sv);
            }
        }

        return;
    }
    if (notify(NotifyMessage_model_register_is_present(notify_message))) {
        return;
    }
    if (notify(NotifyMessage_model_exit_is_present(notify_message))) {
        return;
    }

    /* Notify is currently broadcast to all models at the same model time. */
    notify_spec_t notify_data = {
        .adapter = adapter,
        .message = notify_message,
        .notifyrecv_ts = get_timespec_now(),
    };
    hashmap_iterator(&adapter->models, notify_model, true, &notify_data);
}


void adapter_msg_destroy(Adapter* adapter)
{
    assert(adapter);
    if (adapter->vtable == NULL) return;

    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_clear(&v->builder);
    free(v->ep_buffer);
    v->ep_buffer = NULL;
}


AdapterVTable* adapter_create_msg_vtable(void)
{
    AdapterMsgVTable* v = calloc(1, sizeof(AdapterMsgVTable));

    /* Adapter interface. */
    v->vtable.connect = adapter_msg_connect;
    v->vtable.register_ = adapter_msg_register;
    v->vtable.ready = adapter_msg_model_ready;
    v->vtable.start = adapter_msg_model_start;
    v->vtable.exit = adapter_msg_exit;
    v->vtable.destroy = adapter_msg_destroy;
    /* Extension, adapter msg interface.*/
    v->handle_notify_message = handle_notify_message;
    /* Supporting data objects. */
    flatcc_builder_init(&v->builder);
    v->builder.buffer_flags |= flatcc_builder_with_size;

    return (AdapterVTable*)v;
}
