// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stdint.h>
#include <msgpack.h>
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
} notify_spec_t;


static void sv_delta_to_msgpack(Channel* channel, msgpack_packer* pk)
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
            log_simbus("    SignalWrite: %u = <binary> (len=%u) [name=%s]",
                sv->uid, sv->bin_size, sv->name);
            /* Indicate the binary object was consumed. */
            sv->bin_size = 0;
        } else if (sv->val != sv->final_val) {
            msgpack_pack_double(pk, sv->final_val);
            log_simbus("    SignalWrite: %u = %f [name=%s]", sv->uid,
                sv->final_val, sv->name);
        }
    }
}

static int notify_encode_sv(void* value, void* data)
{
    AdapterModel*     am = value;
    notify_spec_t*    notify_data = data;
    flatcc_builder_t* builder = notify_data->builder;
    assert(builder);

    msgpack_sbuffer sbuf;
    msgpack_packer  pk;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    /* Called for each model, emit SV to builder vector (already started). */
    for (uint32_t i = 0; i < am->channels_length; i++) {
        Channel* ch = _get_channel_byindex(am, i);
        assert(ch);
        _refresh_index(ch);
        log_simbus("SignalVector --> [%s:%u]", ch->name, am->model_uid);

        msgpack_sbuffer_clear(&sbuf);
        sv_delta_to_msgpack(ch, &pk);

        flatbuffers_string_ref_t sv_name =
            flatbuffers_string_create_str(builder, ch->name);
        flatbuffers_uint8_vec_ref_t sv_msgpack_data =
            flatbuffers_uint8_vec_create(
                builder, (uint8_t*)sbuf.data, sbuf.size);
        notify(SignalVector_ref_t) sv = notify(SignalVector_create(
            builder, sv_name, am->model_uid, sv_msgpack_data));
        notify(SignalVector_vec_push(builder, sv));
        log_simbus("    data payload: %lu bytes", sbuf.size);
    }

    msgpack_sbuffer_destroy(&sbuf);
    return 0;
}

static int notify_encode_model(void* value, void* data)
{
    AdapterModel*     am = value;
    notify_spec_t*    notify_data = data;
    flatcc_builder_t* builder = notify_data->builder;
    assert(builder);

    flatbuffers_uint32_vec_push(builder, &am->model_uid);

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
    flatcc_builder_t* builder = &(v->builder);
    ns(MessageType_union_ref_t) message;

    for (uint32_t channel_index = 0; channel_index < am->channels_length;
         channel_index++) {
        Channel* ch = _get_channel_byindex(am, channel_index);

        /* ModelRegister (without 'create') */
        flatcc_builder_reset(builder);
        ns(ModelRegister_start)(builder);
        ns(ModelRegister_step_size_add)(builder, sim->step_size);
        message =
            ns(MessageType_as_ModelRegister(ns(ModelRegister_end)(builder)));
        int rc = 0;
        for (int i = 0; i < retry_count; i++) {
            log_simbus("ModelRegister --> [%s]", ch->name);
            log_simbus("    model_uid=%d", am->model_uid);
            log_simbus("    step_size=%f", sim->step_size);
            rc = send_message(
                adapter, ch->endpoint_channel, am->model_uid, message, true);
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
    flatcc_builder_t* builder = &(v->builder);

    /* SignalIndex on all channels. */
    for (uint32_t channel_index = 0; channel_index < am->channels_length;
         channel_index++) {
        Channel* ch = _get_channel_byindex(am, channel_index);
        ns(MessageType_union_ref_t) message;

        _refresh_index(ch);
        uint32_t signal_list_length = ch->index.count;

        /* SignalIndex with SignalLookup */
        log_simbus("SignalIndex --> [%s]", ch->name);
        flatcc_builder_reset(builder);

        /* SignalLookups, encode into a vector */
        ns(SignalLookup_ref_t)* signal_lookup_list =
            calloc(signal_list_length, sizeof(ns(SignalLookup_ref_t)));

        for (uint32_t i = 0; i < signal_list_length; i++) {
            SignalValue* sv = _get_signal_value_byindex(ch, i);

            flatbuffers_string_ref_t signal_name;
            signal_name = flatbuffers_string_create_str(builder, sv->name);
            ns(SignalLookup_start(builder));
            ns(SignalLookup_name_add(builder, signal_name));
            signal_lookup_list[i] = ns(SignalLookup_end(builder));
            log_simbus("    SignalLookup: %s [UID=%u]", sv->name, sv->uid);
        }
        ns(SignalLookup_vec_ref_t) signal_lookup_vector;
        ns(SignalLookup_vec_start(builder));
        for (uint32_t i = 0; i < signal_list_length; i++)
            ns(SignalLookup_vec_push(builder, signal_lookup_list[i]));
        signal_lookup_vector = ns(SignalLookup_vec_end(builder));

        /* Construct the final message */
        message = ns(MessageType_as_SignalIndex(
            ns(SignalIndex_create(builder, signal_lookup_vector))));
        send_message(
            adapter, ch->endpoint_channel, am->model_uid, message, false);
        free(signal_lookup_list);

        /* Wait on SignalIndex (and handle SignalValue) */
        log_debug("adapter_register: wait on SignalIndex ...");
        {
            const char* msg_channel_name = NULL;
            bool        found = false;
            wait_message(adapter, &msg_channel_name,
                ns(MessageType_SignalIndex), 0, &found);
            if (!found) {
                log_fatal("No SignalIndex received from SimBus!!!");
            }
            assert(msg_channel_name);
        }

        /* SignalRead */
        log_simbus("SignalRead --> [%s]", ch->name);
        flatcc_builder_reset(builder);

        /* Encode the MsgPack payload: data:[ubyte] = [[SignalUID]] */
        msgpack_sbuffer sbuf;
        msgpack_packer  pk;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
        msgpack_pack_array(&pk, 1);
        msgpack_pack_array(&pk, signal_list_length);
        for (unsigned int i = 0; i < signal_list_length; i++) {
            SignalValue* sv = _get_signal_value_byindex(ch, i);
            if (sv->uid) {
                msgpack_pack_uint32(&pk, sv->uid);
                log_simbus("    SignalRead: %u [name=%s]", sv->uid, sv->name);
            }
        }
        log_simbus("    data payload: %lu bytes", sbuf.size);

        /* Construct the final message */
        flatbuffers_uint8_vec_ref_t data_vector;
        data_vector = flatbuffers_uint8_vec_create(
            builder, (uint8_t*)sbuf.data, sbuf.size);
        message = ns(MessageType_as_SignalRead(
            ns(SignalRead_create(builder, data_vector))));
        send_message(
            adapter, ch->endpoint_channel, am->model_uid, message, false);

        /* Wait on SignalIndex (and handle SignalValue) after SignalRead */
        log_debug("adapter_register: wait on SignalValue after SignalRead ...");
        {
            const char* msg_channel_name = NULL;
            bool        found = false;
            wait_message(adapter, &msg_channel_name,
                ns(MessageType_SignalValue), 0, &found);
            if (!found) {
                log_fatal("No SignalValue received from SimBus!!!");
            }
            assert(msg_channel_name);
        }

        msgpack_sbuffer_destroy(&sbuf);
    }

    return 0;
}


static int adapter_msg_model_ready(AdapterModel* am)
{
    Adapter*          adapter = am->adapter;
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* builder = &(v->builder);
    notify_spec_t     notify_data = {
            .adapter = adapter,
            .builder = builder,
    };

    flatcc_builder_reset(builder);

    log_simbus("Notify/ModelReady --> [...]");
    log_simbus("    model_time=%f", am->model_time);

    /* SignalVector vector. */
    notify(SignalVector_vec_start(builder));
    hashmap_iterator(&adapter->models, notify_encode_sv, true, &notify_data);
    notify(SignalVector_vec_ref_t) signals =
        notify(SignalVector_vec_end(builder));

    /* Notify model_uid vector. */
    flatbuffers_uint32_vec_start(builder);
    hashmap_iterator(&adapter->models, notify_encode_model, true, &notify_data);
    flatbuffers_uint32_vec_ref_t model_uids =
        flatbuffers_uint32_vec_end(builder);

    /* NotifyMessage message. */
    notify(NotifyMessage_start(builder));
    notify(NotifyMessage_signals_add(builder, signals));
    notify(NotifyMessage_model_uid_add(builder, model_uids));
    notify(NotifyMessage_model_time_add(builder, am->model_time));
    notify(
        NotifyMessage_bench_model_time_ns_add(builder, am->bench_steptime_ns));
    notify(NotifyMessage_bench_notify_time_ns_add(builder,
        get_elapsedtime_ns(am->bench_notifyrecv_ts) - am->bench_steptime_ns));
    notify(NotifyMessage_ref_t) message = notify(NotifyMessage_end(builder));
    send_notify_message(adapter, message);

    return 0;
}


static int adapter_msg_model_start(AdapterModel* am)
{
    Adapter* adapter = am->adapter;

    /* Wait on Notify. Notify will contain all channel/signal values.
     * Indicated by parameters channel_name and message_type being NULL/0. */
    log_debug("adapter_ready: wait on Notify ...");
    bool found = false;
    int  rc = 0;

    rc = wait_message(adapter, NULL, 0, 0, &found);
    if (rc != 0) {
        log_error("wait_message returned %d", rc);

        /* Handle specific error conditions. */
        if (rc == ETIME) return rc; /* TIMEOUT */
    }

    if (0) {
        /* Wait on ModelStart from all channels (and handle SignalValue,
         * ParameterValue) */
        uint32_t start_counter = 0;
        log_debug("adapter_ready: wait on ModelStart ...");
        while (start_counter < am->channels_length) {
            const char* msg_channel_name = NULL;
            bool        found = false;
            int         rc = wait_message(adapter, &msg_channel_name,
                        ns(MessageType_ModelStart), 0, &found);
            if (rc != 0) {
                log_error("wait_message returned %d", rc);

                /* Handle specific error conditions. */
                if (rc == ETIME) return rc; /* TIMEOUT */
            }
            if (found) start_counter++;
        }
    }

    return 0;
}


static int adapter_msg_exit(AdapterModel* am)
{
    Adapter*          adapter = am->adapter;
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* builder = &(v->builder);
    ns(MessageType_union_ref_t) message;

    for (uint32_t channel_index = 0; channel_index < am->channels_length;
         channel_index++) {
        Channel* ch = _get_channel_byindex(am, channel_index);
        log_simbus("ModelExit --> [%s]", ch->name);
        /* ModelExit */
        flatcc_builder_reset(builder);
        message = ns(MessageType_as_ModelExit(ns(ModelExit_create(builder))));
        send_message(
            adapter, ch->endpoint_channel, am->model_uid, message, false);
    }

    return 0;
}


/*
Adapter Message Handling/Processing
-----------------------------------
*/

static void process_signal_value_data(
    Channel* channel, flatbuffers_uint8_vec_t data_vector, size_t length)
{
    _refresh_index(channel);

    /* Unpack. */
    bool             result;
    msgpack_unpacker unpacker;
    // +4 is COUNTER_SIZE.
    result = msgpack_unpacker_init(&unpacker, length + 4);
    if (!result) {
        log_error("MsgPack unpacker init failed!");
        goto error_clean_up;
    }
    if (msgpack_unpacker_buffer_capacity(&unpacker) < length) {
        log_error("MsgPack unpacker buffer size too small!");
        goto error_clean_up;
    }
    memcpy(
        msgpack_unpacker_buffer(&unpacker), (const char*)data_vector, length);
    msgpack_unpacker_buffer_consumed(&unpacker, length);

    /* Unpack: Process Objects. */
    msgpack_unpacked      unpacked;
    msgpack_unpack_return ret;
    msgpack_unpacked_init(&unpacked);
    ret = msgpack_unpacker_next(&unpacker, &unpacked);
    if (ret != MSGPACK_UNPACK_SUCCESS) {
        log_simbus("WARNING: data vector unpacked with unexpected return code! "
                   "(ret=%d)",
            ret);
        log_error("MsgPack msgpack_unpacker_next failed!");
        goto error_clean_up;
    }

    /* Root object is array with 2 elements. */
    msgpack_object obj = unpacked.data;
    // debug_msgpack_print(obj);
    if (obj.type != MSGPACK_OBJECT_ARRAY) {
        log_simbus(
            "WARNING: data vector with unexpected object type! (%d)", obj.type);
        goto error_clean_up;
    }
    if (obj.via.array.size != 2) {
        log_simbus(
            "WARNING: data vector with wrong size! (%d)", obj.via.array.size);
        goto error_clean_up;
    }
    /* Extract the embedded UID and Value arrays. */
    msgpack_object uid_obj = obj.via.array.ptr[0];
    msgpack_object val_obj = obj.via.array.ptr[1];
    if (uid_obj.type != MSGPACK_OBJECT_ARRAY) {
        log_simbus(
            "WARNING: signal_uid array with unexpected object type! (%d)",
            uid_obj.type);
        goto error_clean_up;
    }
    if (val_obj.type != MSGPACK_OBJECT_ARRAY) {
        log_simbus("WARNING: signal value array unexpected object type! (%d)",
            val_obj.type);
        goto error_clean_up;
    }
    if (uid_obj.via.array.size != val_obj.via.array.size) {
        log_simbus(
            "WARNING: signal uid and value arrays are different length!");
        goto error_clean_up;
    }

    /* Update the SignalValue for each included Signal UID:Value pair. */
    for (uint32_t i = 0; i < uid_obj.via.array.size; i++) {
        uint32_t    _uid = uid_obj.via.array.ptr[i].via.u64;
        double      _value = 0;
        const void* _bin_ptr = NULL;
        uint32_t    _bin_size = 0;
        switch (val_obj.via.array.ptr[i].type) {
        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            _value = val_obj.via.array.ptr[i].via.u64;
            break;
        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            _value = val_obj.via.array.ptr[i].via.i64;
            break;
        case MSGPACK_OBJECT_FLOAT32:
            _value = (float)val_obj.via.array.ptr[i].via.f64;
            break;
        case MSGPACK_OBJECT_FLOAT64:
            _value = val_obj.via.array.ptr[i].via.f64;
            break;
        case MSGPACK_OBJECT_BIN:
            _bin_ptr = val_obj.via.array.ptr[i].via.bin.ptr;
            _bin_size = val_obj.via.array.ptr[i].via.bin.size;
            break;
        default:
            log_simbus("WARNING: signal value unexpected type! (%d)",
                val_obj.via.array.ptr[i].type);
        }
        SignalValue* sv = _find_signal_by_uid(channel, _uid);
        if ((sv == NULL) && _uid) {
            log_simbus("WARNING: signal with uid (%u) not found!", _uid);
            continue;
        }
        if (_bin_size) {
            /* Binary. */
            dse_buffer_append(&sv->bin, &sv->bin_size, &sv->bin_buffer_size,
                _bin_ptr, _bin_size);
            log_simbus("    SignalValue: %u = <binary> (len=%u) [name=%s]",
                _uid, sv->bin_size, sv->name);
        } else {
            /* Double. */
            sv->val = _value;
            sv->final_val =
                _value; /* Reset final_val (changes will trigger SignalWrite) */
            log_simbus(
                "    SignalValue: %u = %f [name=%s]", _uid, sv->val, sv->name);
        }
    }

error_clean_up:
    /* Unpack: Cleanup. */
    msgpack_unpacked_destroy(&unpacked);
    msgpack_unpacker_destroy(&unpacker);
}

static void process_signal_value_message(
    Channel* channel, ns(SignalValue_table_t) signal_value_table)
{
    if (!ns(SignalValue_data_is_present(signal_value_table))) {
        log_simbus("WARNING: no data in SignalValue message!");
        return;
    }

    /* Decode the MsgPack payload: data:[ubyte] = [[UID:0..N],[Value:0..N]] */
    flatbuffers_uint8_vec_t data_vector =
        ns(SignalValue_data(signal_value_table));
    size_t length = flatbuffers_uint8_vec_len(data_vector);
    if (data_vector == NULL) {
        log_simbus(
            "WARNING: data vector could not be obtained SignalValue message!");
        return;
    }
    log_simbus("    data payload: %lu bytes", length);

    process_signal_value_data(channel, data_vector, length);
}

static void process_signal_index_message(
    Channel* channel, ns(SignalIndex_table_t) signal_index_table)
{
    if (!ns(SignalIndex_indexes_is_present(signal_index_table))) {
        log_simbus("WARNING: no indexes in SignalIndex message!");
        return;
    }
    /* Decode the SignalLookup vector. */
    _refresh_index(channel);
    ns(SignalLookup_vec_t) vector = ns(SignalIndex_indexes(signal_index_table));
    size_t vector_len = ns(SignalLookup_vec_len(vector));
    for (uint32_t _vi = 0; _vi < vector_len; _vi++) {
        /* Check the Lookup data is complete. */
        ns(SignalLookup_table_t) signal_lookup =
            ns(SignalLookup_vec_at(vector, _vi));
        if (!ns(SignalLookup_name_is_present(signal_lookup))) continue;
        // if (!ns(SignalLookup_signal_uid_is_present(signal_lookup))) continue;
        const char* signal_name = ns(SignalLookup_name(signal_lookup));
        uint32_t    signal_uid = ns(SignalLookup_signal_uid(signal_lookup));
        log_simbus("    SignalLookup: %s [UID=%u]", signal_name, signal_uid);
        if (signal_uid == 0) continue;
        /* Update the Adapter signal_value array. */
        for (unsigned int i = 0; i < channel->index.count; i++) {
            SignalValue* sv = channel->index.map[i].signal;
            if (strcmp(sv->name, signal_name) == 0) {
                sv->uid = signal_uid;
                break;
            }
        }
    }
}

static void process_model_start_message(AdapterModel* am, Channel* channel,
    ns(ModelStart_table_t) model_start_table)
{
    am->model_time = ns(ModelStart_model_time(model_start_table));
    am->stop_time = ns(ModelStart_stop_time(model_start_table));
    if (am->stop_time <= am->model_time) {
        log_error("WARNING:stop_time is NOT greater than model_time!");
    }
    log_simbus("    model_time=%f", am->model_time);
    log_simbus("    stop_time=%f", am->stop_time);

    /* Handle embedded SignalValue message. */
    if (ns(ModelStart_signal_value_is_present(model_start_table))) {
        process_signal_value_message(
            channel, ns(ModelStart_signal_value(model_start_table)));
    }
}

static void handle_channel_message(Adapter* adapter, const char* channel_name,
    ns(ChannelMessage_table_t) channel_message, int32_t          token)
{
    UNUSED(token);
    assert(adapter);
    assert(channel_name);

    uint32_t message_model_uid = ns(ChannelMessage_model_uid(channel_message));
    char     hash_key[UID_KEY_LEN];
    snprintf(hash_key, UID_KEY_LEN - 1, "%d", message_model_uid);
    AdapterModel* am = hashmap_get(&adapter->models, hash_key);
    assert(am);
    Channel* channel = hashmap_get(&am->channels, channel_name);
    assert(channel);

    ns(MessageType_union_type_t) msg_type;
    msg_type = ns(ChannelMessage_message_type(channel_message));

    /* ModelRegister, normally ignore, debug only. */
    if (msg_type == ns(MessageType_ModelRegister)) {
        ns(ModelRegister_table_t) sr =
            ns(ChannelMessage_message(channel_message));
        log_simbus("ModelRegister <-- [%s]", channel->name);
        log_simbus("    step_size=%f", ns(ModelRegister_step_size(sr)));
    }
    /* SignalIndex */
    else if (msg_type == ns(MessageType_SignalIndex)) {
        log_simbus("SignalIndex <-- [%s]", channel->name);
        process_signal_index_message(
            channel, ns(ChannelMessage_message(channel_message)));
    }
    /* SignalValue */
    else if (msg_type == ns(MessageType_SignalValue)) {
        log_simbus("SignalValue <-- [%s]", channel->name);
        process_signal_value_message(
            channel, ns(ChannelMessage_message(channel_message)));
    }
    /* ModelStart */
    else if (msg_type == ns(MessageType_ModelStart)) {
        log_simbus("ModelStart <-- [%s]", channel->name);
        process_model_start_message(
            am, channel, ns(ChannelMessage_message(channel_message)));
    }
    /* ModelExit */
    else if (msg_type == ns(MessageType_ModelExit)) {
        log_simbus("ModelExit <-- [%s]", channel->name);
    }
    /* Unexpected message? */
    else {
        log_simbus("UNEXPECTED <-- [%s]", channel->name);
        log_simbus("    message_type (%d)", msg_type);
    }
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
        log_error("WARNING:stop_time is NOT greater than model_time!");
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

        flatbuffers_uint8_vec_t data_vector =
            notify(SignalVector_data(signal_vector));
        size_t data_length = flatbuffers_uint8_vec_len(data_vector);
        if (data_vector == NULL) {
            log_simbus("WARNING: signal vector data not provided!");
            continue;
        }
        log_simbus("    data payload: %lu bytes", data_length);

        /* Process the Signal Vector*/
        Channel* channel = hashmap_get(&am->channels, channel_name);
        if (channel == NULL) continue;
        log_simbus("SignalVector <-- [%s]", channel->name);
        process_signal_value_data(channel, data_vector, data_length);
    }

    return 0;
}

static void handle_notify_message(
    Adapter* adapter, notify(NotifyMessage_table_t) notify_message)
{
    /* Notify is currently broadcast to all models at the same model time. */
    notify_spec_t notify_data = {
        .adapter = adapter,
        .message = notify_message,
        .notifyrecv_ts = get_timespec_now(),
    };
    hashmap_iterator(&adapter->models, notify_model, true, &notify_data);
}


/*
Adapter VTable Create
---------------------
*/

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
    v->handle_message = handle_channel_message;
    v->handle_notify_message = handle_notify_message;
    /* Supporting data objects. */
    flatcc_builder_init(&v->builder);
    v->builder.buffer_flags |= flatcc_builder_with_size;

    return (AdapterVTable*)v;
}
