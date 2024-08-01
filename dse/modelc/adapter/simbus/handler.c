// Copyright 2023, 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <msgpack.h>
#include <dse/logger.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/adapter/simbus/simbus_private.h>
#include <dse/modelc/adapter/private.h>
#include <dse/modelc/adapter/timer.h>
#include <dse/modelc/adapter/message.h>


#define UNUSED(x) ((void)x)


static uint32_t _process_signal_lookup(Channel* ch, const char* signal_name)
{
    /* NOTE: may create new SignalValue objects, call
       _refresh_index() after processing a SignalIndex message to
       update the internal indexes. */

    /* Search for signal name, if missing will be created. */
    SignalValue* sv = _get_signal_value(ch, signal_name);
    if (sv->uid == 0) sv->uid = simbus_generate_uid_hash(sv->name);
    log_simbus("    SignalLookup: %s [UID=%u]", signal_name, sv->uid);
    return sv->uid;
}


static void process_signal_index_message(Adapter* adapter, Channel* channel,
    uint32_t model_uid, ns(SignalIndex_table_t) signal_index_table)
{
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* builder = &(v->builder);

    flatcc_builder_reset(builder);

    /* Extract the vector parameters and create response object. */
    if (!ns(SignalIndex_indexes_is_present(signal_index_table))) {
        log_simbus("WARNING: no indexes in SignalIndex message!");
    }
    ns(SignalLookup_vec_t) vector = ns(SignalIndex_indexes(signal_index_table));
    size_t vector_len = ns(SignalLookup_vec_len(vector));
    ns(SignalLookup_ref_t)* resp__signal_lookup_list =
        calloc(vector_len, sizeof(ns(SignalLookup_ref_t)));

    /* Do the lookup and build the response vector. */
    for (uint32_t _vi = 0; _vi < vector_len; _vi++) {
        /* Check the Lookup data is complete. */
        ns(SignalLookup_table_t) signal_lookup =
            ns(SignalLookup_vec_at(vector, _vi));
        if (!ns(SignalLookup_name_is_present(signal_lookup))) continue;
        const char* signal_name = ns(SignalLookup_name(signal_lookup));
        uint32_t    signal_uid = ns(SignalLookup_signal_uid(signal_lookup));
        /* Lookup/Resolve the signal (the provided UID is discarded). */
        signal_uid = _process_signal_lookup(channel, signal_name);
        /* Create the response Lookup table/object. */
        flatbuffers_string_ref_t resp__signal_name;
        resp__signal_name = flatbuffers_string_create_str(builder, signal_name);
        ns(SignalLookup_start(builder));
        ns(SignalLookup_name_add(builder, resp__signal_name));
        ns(SignalLookup_signal_uid_add(builder, signal_uid));
        resp__signal_lookup_list[_vi] = ns(SignalLookup_end(builder));
    }
    _refresh_index(channel);

    /* Create the response Lookup vecotr. */
    log_simbus("SignalIndex --> [%s]", channel->name);
    ns(SignalLookup_vec_ref_t) resp__signal_lookup_vector;
    ns(SignalLookup_vec_start(builder));
    for (uint32_t i = 0; i < vector_len; i++)
        ns(SignalLookup_vec_push(builder, resp__signal_lookup_list[i]));
    resp__signal_lookup_vector = ns(SignalLookup_vec_end(builder));
    /* Construct the response SignalIndex message. */
    ns(MessageType_union_ref_t) resp__message;
    resp__message = ns(MessageType_as_SignalIndex(
        ns(SignalIndex_create(builder, resp__signal_lookup_vector))));
    send_message(
        adapter, channel->endpoint_channel, model_uid, resp__message, false);
    free(resp__signal_lookup_list);
}

static void process_signal_read_message(Adapter* adapter, Channel* channel,
    uint32_t model_uid, ns(SignalRead_table_t) signal_read_table)
{
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* builder = &(v->builder);

    flatcc_builder_reset(builder);
    uint32_t read_signal_count = 0;  // Incase of unpack error, return NOP.

    /* Extract the vector parameter. */
    if (!ns(SignalRead_data_is_present(signal_read_table))) {
        log_simbus("WARNING: no data in SignalRead message!");
    }

    /* Decode the MsgPack payload: data:[ubyte] = [[UID:0..N]] */
    flatbuffers_uint8_vec_t data_vector =
        ns(SignalRead_data(signal_read_table));
    size_t data_length = flatbuffers_uint8_vec_len(data_vector);
    if (data_vector == NULL) {
        log_simbus(
            "WARNING: data vector could not be obtained SignalRead message!");
        data_length = 0;
    }
    log_simbus("    data payload: %lu bytes", data_length);
    /* Unpack. */
    bool             result;
    msgpack_unpacker unpacker;
    //  +4 is COUNTER_SIZE.
    result = msgpack_unpacker_init(&unpacker, data_length + 4);
    if (!result) {
        log_error("MsgPack unpacker init failed!");
        goto error_clean_up;
    }
    if (msgpack_unpacker_buffer_capacity(&unpacker) < data_length) {
        log_error("MsgPack unpacker buffer size too small!");
        goto error_clean_up;
    }
    memcpy(msgpack_unpacker_buffer(&unpacker), (const char*)data_vector,
        data_length);
    msgpack_unpacker_buffer_consumed(&unpacker, data_length);
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
    /* Root object is array with 1 (but 2 would also be possible). */
    msgpack_object obj = unpacked.data;
    // debug_msgpack_print(obj);
    if (obj.type != MSGPACK_OBJECT_ARRAY) {
        log_simbus(
            "WARNING: data vector with unexpected object type! (%d)", obj.type);
    }
    if (obj.via.array.size != 1) {
        log_simbus(
            "WARNING: data vector with wrong size! (%d)", obj.via.array.size);
        if (obj.via.array.size == 0) goto error_clean_up;
    }
    /* Extract the embedded UID array. */
    msgpack_object uid_obj = obj.via.array.ptr[0];
    if (uid_obj.type == MSGPACK_OBJECT_ARRAY) {
        read_signal_count = uid_obj.via.array.size;
        log_simbus("    read_signal_count %u", read_signal_count);
        for (uint32_t i = 0; i < read_signal_count; i++) {
            uint32_t     _uid = uid_obj.via.array.ptr[i].via.u64;
            SignalValue* sv = _find_signal_by_uid(channel, _uid);
            if (sv == NULL) {
                log_simbus("WARNING: signal with uid (%u) not found!", _uid);
            } else {
                log_simbus("    SignalRead: %s [UID=%u]", sv->name, _uid);
            }
        }
    } else {
        log_simbus(
            "WARNING: signal_uid array with unexpected object type! (%d)",
            obj.type);
    }

    /* Encode SignalValue MsgPack payload: data:[ubyte] = [[UID],[Value]] */
    log_simbus("SignalValue --> [%s]", channel->name);
    log_simbus("    model_uid=%d", model_uid);
    msgpack_sbuffer sbuf;
    msgpack_packer  pk;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    /* First(root) Object, array, 2 elements. */
    msgpack_pack_array(&pk, 2);
    /* 1st Object in root Array, list of UID's. */
    msgpack_pack_array(&pk, read_signal_count);
    for (uint32_t i = 0; i < read_signal_count; i++) {
        uint32_t _uid = uid_obj.via.array.ptr[i].via.u64;
        if (_uid) {
            msgpack_pack_uint32(&pk, _uid);
        }
    }
    /* 2st Object in root Array, list of Values. */
    msgpack_pack_array(&pk, read_signal_count);
    for (uint32_t i = 0; i < read_signal_count; i++) {
        uint32_t _uid = uid_obj.via.array.ptr[i].via.u64;
        if (_uid) {
            SignalValue* sv = _find_signal_by_uid(channel, _uid);
            if (sv == NULL) {
                log_simbus("WARNING: signal with uid (%u) not found!", _uid);
                msgpack_pack_double(&pk, 0.0);
                continue;
            }
            if (sv->bin) {
                /* The Signal Read case will return return an empty binary
                 * blob because the binary data will only be sent as
                 * SignalValue message embedded in ModelStart message. That
                 * ensures that the binary stream is only sent when it is
                 * fully constructed from all previous ModelReady messages
                 * from all connected models (via embedded SignalWrite message).
                 */
                msgpack_pack_bin_with_body(&pk, sv->bin, 0);
                log_simbus("    SignalWrite: %u = <binary> (len=%u) [name=%s]",
                    sv->uid, 0, sv->name);
            } else {
                msgpack_pack_double(&pk, sv->val);
                log_simbus("    uid=%u, val=%f", _uid, sv->val);
            }
        }
    }

    /* Construct the SignalValue message. */
    log_simbus("    data payload: %lu bytes", sbuf.size);
    ns(MessageType_union_ref_t) resp__message;
    flatbuffers_uint8_vec_ref_t resp__data_vector;
    resp__data_vector =
        flatbuffers_uint8_vec_create(builder, (uint8_t*)sbuf.data, sbuf.size);
    resp__message = ns(MessageType_as_SignalValue(
        ns(SignalValue_create(builder, resp__data_vector))));
    send_message(
        adapter, channel->endpoint_channel, model_uid, resp__message, false);

error_clean_up:
    /* Unpack: Cleanup. */
    msgpack_unpacked_destroy(&unpacked);
    msgpack_unpacker_destroy(&unpacker);
    msgpack_sbuffer_destroy(&sbuf);
}


static void process_signalvector(
    Channel* channel, flatbuffers_uint8_vec_t data_vector)
{
    /* Decode the MsgPack payload: data:[ubyte] = [[UID:0..N],[Value:0..N]] */
    if (data_vector == NULL) {
        log_simbus(
            "WARNING: data vector could not be obtained SignalWrite message!");
        return;
    }
    size_t           length = flatbuffers_uint8_vec_len(data_vector);
    /* Unpack. */
    bool             result;
    msgpack_unpacker unpacker;
    // 4 is COUNTER_SIZE.
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
    uint32_t write_signal_count = uid_obj.via.array.size;
    for (uint32_t i = 0; i < write_signal_count; i++) {
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
            sv->final_val =
                _value; /* Reset final_val (changes will trigger SignalWrite) */
            log_simbus("    SignalWrite: %u = %f [name=%s, prev=%f]", _uid,
                sv->final_val, sv->name, sv->val);
        }
    }

error_clean_up:
    /* Unpack: Cleanup. */
    msgpack_unpacked_destroy(&unpacked);
    msgpack_unpacker_destroy(&unpacker);
}


static void process_signal_write_message(Adapter* adapter, Channel* channel,
    uint32_t model_uid, ns(SignalWrite_table_t) signal_write_table)
{
    UNUSED(adapter);
    UNUSED(model_uid);

    if (!ns(SignalWrite_data_is_present(signal_write_table))) {
        log_simbus("WARNING: no data in SignalWrite message!");
        return;
    }

    /* Decode the MsgPack payload: data:[ubyte] = [[UID:0..N],[Value:0..N]] */
    flatbuffers_uint8_vec_t data_vector =
        ns(SignalWrite_data(signal_write_table));
    if (data_vector == NULL) {
        log_simbus(
            "WARNING: data vector could not be obtained SignalWrite message!");
        return;
    }
    process_signalvector(channel, data_vector);
}


static void process_model_ready_message(Adapter* adapter, Channel* channel,
    uint32_t model_uid, ns(ModelReady_table_t) model_ready_table)
{
    if (ns(ModelReady_model_time_is_present(model_ready_table))) {
        double model_time = ns(ModelReady_model_time(model_ready_table));
        log_simbus("    model_time=%f", model_time);
        /** Currently model_time is ignored. The Bus is progressing time
        according to its own step_size for _all_ models.
        */
    }

    /* Handle embedded SignalWite message. */
    if (ns(ModelReady_signal_write_is_present(model_ready_table))) {
        process_signal_write_message(adapter, channel, model_uid,
            ns(ModelReady_signal_write(model_ready_table)));
    }
}


static void process_notify_signalvector(Adapter* adapter, Channel* channel,
    uint32_t model_uid, notify(SignalVector_table_t) signal_vector)
{
    UNUSED(adapter);
    UNUSED(model_uid);

    /* Handle SignalVector. */
    if (!notify(SignalVector_data_is_present(signal_vector))) {
        log_simbus("WARNING: no data in SignalWrite message!");
        return;
    }

    /* Decode the MsgPack payload: data:[ubyte] = [[UID:0..N],[Value:0..N]] */
    flatbuffers_uint8_vec_t data_vector =
        notify(SignalVector_data(signal_vector));
    if (data_vector == NULL) {
        log_simbus(
            "WARNING: data vector could not be obtained SignalVector table!");
        return;
    }
    process_signalvector(channel, data_vector);
}


static void sv_delta_to_msgpack(Channel* channel, msgpack_packer* pk)
{
    log_simbus("SignalVector --> [%s]", channel->name);

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
            log_simbus("    SignalValue: %u = <binary> (len=%u) [name=%s]",
                sv->uid, sv->bin_size, sv->name);
        } else if (sv->val != sv->final_val) {
            msgpack_pack_double(pk, sv->final_val);
            log_simbus("    SignalValue: %u = %f [name=%s]", sv->uid,
                sv->final_val, sv->name);
        }
    }
}


static void resolve_channel(Channel* channel)
{
    for (uint32_t i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        sv->val = sv->final_val;
        sv->bin_size = 0;
    }
}


static void resolve_and_notify(
    Adapter* adapter, double model_time, double schedule_time)
{
    AdapterModel*     am = adapter->bus_adapter_model;
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* builder = &(v->builder);
    msgpack_sbuffer   sbuf;
    msgpack_packer    pk;

    flatcc_builder_reset(builder);
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    log_simbus("Notify/ModelStart --> [...]");
    log_simbus("    model_time=%f", model_time);
    log_simbus("    schedule_time=%f", schedule_time);

    /* SignalVector vector. */
    notify(SignalVector_vec_start(builder));
    for (uint32_t i = 0; i < am->channels_length; i++) {
        Channel* ch = _get_channel_byindex(am, i);
        _refresh_index(ch);

        msgpack_sbuffer_clear(&sbuf);
        sv_delta_to_msgpack(ch, &pk);
        resolve_channel(ch);

        flatbuffers_string_ref_t sv_name =
            flatbuffers_string_create_str(builder, ch->name);
        flatbuffers_uint8_vec_ref_t sv_msgpack_data =
            flatbuffers_uint8_vec_create(
                builder, (uint8_t*)sbuf.data, sbuf.size);
        notify(SignalVector_ref_t) sv =
            notify(SignalVector_create(builder, sv_name, 0, sv_msgpack_data));
        notify(SignalVector_vec_push(builder, sv));
        log_simbus("    data payload: %lu bytes", sbuf.size);
    }
    notify(SignalVector_vec_ref_t) signals =
        notify(SignalVector_vec_end(builder));

    /* NotifyMessage message. */
    notify(NotifyMessage_start(builder));
    notify(NotifyMessage_signals_add(builder, signals));
    notify(NotifyMessage_model_time_add(builder, model_time));
    notify(NotifyMessage_schedule_time_add(builder, schedule_time));
    notify(NotifyMessage_ref_t) message = notify(NotifyMessage_end(builder));
    send_notify_message(adapter, message);
    msgpack_sbuffer_destroy(&sbuf);
}


static void resolve_channel_and_model_start(
    Adapter* adapter, Channel* channel, double model_time, double stop_time)
{
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* builder = &(v->builder);

    log_simbus("SignalValue+", channel->name);

    /* Encode SignalWrite MsgPack payload: data:[ubyte] = [[UID],[Value]] */
    msgpack_sbuffer sbuf;
    msgpack_packer  pk;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    /* First(root) Object, array, 2 elements. */
    msgpack_pack_array(&pk, 2);
    uint32_t changed_signal_count = 0;
    for (uint32_t i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        if (sv->uid == 0) continue;
        if ((sv->val != sv->final_val) || (sv->bin && sv->bin_size)) {
            changed_signal_count++;
        }
    }
    /* 1st Object in root Array, list of UID's. */
    msgpack_pack_array(&pk, changed_signal_count);
    for (uint32_t i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        if (sv->uid == 0) continue;
        if ((sv->val != sv->final_val) || (sv->bin && sv->bin_size)) {
            msgpack_pack_uint32(&pk, sv->uid);
        }
    }
    /* 2st Object in root Array, list of Values. */
    msgpack_pack_array(&pk, changed_signal_count);
    for (uint32_t i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        if (sv->uid == 0) continue;
        if (sv->bin && sv->bin_size) {
            msgpack_pack_bin_with_body(&pk, sv->bin, sv->bin_size);
            log_simbus("    SignalValue: %u = <binary> (len=%u) [name=%s]",
                sv->uid, sv->bin_size, sv->name);
        } else if (sv->val != sv->final_val) {
            msgpack_pack_double(&pk, sv->final_val);
            log_simbus("    SignalValue: %u = %f [name=%s]", sv->uid,
                sv->final_val, sv->name);
        }
    }
    log_simbus("    data payload: %lu bytes", sbuf.size);

    /* Resolve the Bus. */
    for (uint32_t i = 0; i < channel->index.count; i++) {
        SignalValue* sv = channel->index.map[i].signal;
        sv->val = sv->final_val;
        sv->bin_size = 0;
    }

    /* Send ModelStart with SignalValue. */
    for (uint32_t i = 0; i < channel->model_ready_set->number_nodes; i++) {
        if (channel->model_ready_set->nodes[i] == NULL) continue;
        uint32_t model_uid = (uint32_t)strtol(
            channel->model_ready_set->nodes[i]->_key, NULL, 10);
        flatcc_builder_reset(builder);
        /* SignalValue */
        ns(SignalWrite_ref_t) resp__signal_value_message;
        flatbuffers_uint8_vec_ref_t resp__data_vector;
        resp__data_vector = flatbuffers_uint8_vec_create(
            builder, (uint8_t*)sbuf.data, sbuf.size);
        resp__signal_value_message =
            ns(SignalValue_create(builder, resp__data_vector));
        /* ModelStart */
        log_simbus("ModelStart --> [%s]", channel->name);
        log_simbus("    model_uid=%d", model_uid);
        log_simbus("    model_time=%f", model_time);
        log_simbus("    stop_time=%f", stop_time);
        ns(MessageType_union_ref_t) resp__model_start_message;
        resp__model_start_message =
            ns(MessageType_as_ModelStart(ns(ModelStart_create(builder,
                model_time,                 // model_time
                stop_time,                  // stop_time
                resp__signal_value_message  // signal_write
                ))));
        send_message(adapter, channel->endpoint_channel, model_uid,
            resp__model_start_message, false);
    }
    msgpack_sbuffer_destroy(&sbuf);
}


void simbus_handle_notify_message(
    Adapter* adapter, notify(NotifyMessage_table_t) notify_message)
{
    AdapterModel* am = adapter->bus_adapter_model;

    /* Notify meta information. */
    double model_time = notify(NotifyMessage_model_time(notify_message));
    log_simbus("Notify/ModelReady <--");
    log_simbus("    model_time=%f", model_time);

    /* Benchmarking/Profiling. */
    if (adapter->bus_time > 0.0) {
        struct timespec ref_ts = get_timespec_now();
        uint64_t        model_exec_time =
            notify(NotifyMessage_bench_model_time_ns(notify_message));
        uint64_t model_proc_time =
            notify(NotifyMessage_bench_notify_time_ns(notify_message));
        uint64_t network_time_ns =
            get_deltatime_ns(adapter->bench_notifysend_ts, ref_ts) -
            (model_proc_time + model_exec_time);
        flatbuffers_uint32_vec_t vector =
            notify(NotifyMessage_model_uid(notify_message));
        size_t vector_len = flatbuffers_uint32_vec_len(vector);
        for (uint32_t _vi = 0; _vi < vector_len; _vi++) {
            uint32_t model_uid = flatbuffers_uint32_vec_at(vector, _vi);
            simbus_profile_accumulate_model_part(model_uid, model_exec_time,
                model_proc_time, network_time_ns, ref_ts);
        }
    }

    /* Handle embedded SignalVector tables. */
    notify(SignalVector_vec_t) vector =
        notify(NotifyMessage_signals(notify_message));
    size_t vector_len = notify(SignalVector_vec_len(vector));
    for (uint32_t _vi = 0; _vi < vector_len; _vi++) {
        /* Check the Lookup data is complete. */
        notify(SignalVector_table_t) signal_vector =
            notify(SignalVector_vec_at(vector, _vi));
        if (!notify(SignalVector_name_is_present(signal_vector))) {
            log_error("WARNING: signal vector name not provided!");
            continue;
        }
        if (!notify(SignalVector_model_uid_is_present(signal_vector))) {
            log_error("WARNING: signal vector model_uid not provided!");
            continue;
        }
        const char* channel_name = notify(SignalVector_name(signal_vector));
        uint32_t    model_uid = notify(SignalVector_model_uid(signal_vector));
        log_simbus("SignalVector <-- [%s:%u]", channel_name, model_uid);

        Channel* channel = hashmap_get(&am->channels, channel_name);
        process_notify_signalvector(adapter, channel, model_uid, signal_vector);
        simbus_model_at_ready(am, channel, model_uid);
    }

    /* Resolve the Bus. */
    if (simbus_network_ready(am) && simbus_models_ready(am)) {
        /**
        This condition will exist when the last model on the last channel
        sends its ModelReady message. When that happens, resolve the bus.

        Time on the Bus is progressed according to Bus Cycle Time.

        Increment the bus time via Kahan summation.
        */
        double y = adapter->bus_step_size - adapter->bus_time_correction;
        double t = adapter->bus_time + y;
        adapter->bus_time_correction = (t - adapter->bus_time) - y;
        adapter->bus_time = t;

        double model_time = adapter->bus_time;
        double stop_time = model_time + adapter->bus_step_size;

        /* Benchmarking/Profiling. */
        if (adapter->bus_time > 0.0) {
            struct timespec ref_ts = get_timespec_now();
            uint64_t        simbus_cycle_total_ns =
                get_deltatime_ns(adapter->bench_notifysend_ts, ref_ts);
            simbus_profile_accumulate_cycle_total(
                simbus_cycle_total_ns, ref_ts);
        }
        adapter->bench_notifysend_ts = get_timespec_now();

        /* Notify/ModelStart. */
        resolve_and_notify(adapter, model_time, stop_time);
        simbus_models_to_start(am);
    }
}


void simbus_handle_message(Adapter* adapter, const char* channel_name,
    ns(ChannelMessage_table_t) channel_message, int32_t  token)
{
    assert(adapter);
    assert(channel_name);

    AdapterModel* am = adapter->bus_adapter_model;
    assert(am);
    Channel* channel = hashmap_get(&am->channels, channel_name);
    assert(channel);

    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* builder = &(v->builder);
    int32_t           rc = 0;
    char*             response = NULL;

    /* limitations:
     * All signal changes sent to all models. No filtering based on SignalRead.
     * Signal aliases are not supported.
     * Many other limitations.
     */

    ns(MessageType_union_type_t) msg_type =
        ns(ChannelMessage_message_type(channel_message));
    uint32_t model_uid = ns(ChannelMessage_model_uid(channel_message));

    /* ModelRegister */
    if (msg_type == ns(MessageType_ModelRegister)) {
        ns(ModelRegister_table_t) t =
            ns(ChannelMessage_message(channel_message));
        log_simbus("ModelRegister <-- [%s]", channel->name);
        log_simbus("    model_uid=%d", model_uid);
        log_simbus("    step_size=%f", ns(ModelRegister_step_size(t)));
        log_simbus("    token=%d", token);

        /* Count the number of ModelRegisters. Keep in mind that this message
        will be sent from a model on all channels, therefore the number of
        ModelRegister messages may exceed the number of models.
         */
        simbus_model_at_register(am, channel, model_uid);
        if (simbus_network_ready(am)) {
            log_simbus("Bus Network is complete, all Models connected.");
        }
        /* Send response if necessary. */
        if (token || rc) {
            log_simbus("ModelRegister ACK --> [%s]", channel->name);
            log_simbus("    model_uid=%d", model_uid);
            log_simbus("    token=%d", token);
            log_simbus("    rc=%d", rc);
            ns(MessageType_union_ref_t) ack_message;
            flatcc_builder_reset(builder);
            ns(ModelRegister_start)(builder);
            ack_message = ns(
                MessageType_as_ModelRegister(ns(ModelRegister_end)(builder)));
            send_message_ack(adapter, channel->endpoint_channel, model_uid,
                ack_message, token, rc, response);
        }
    }
    /* SignalIndex */
    else if (msg_type == ns(MessageType_SignalIndex)) {
        log_simbus("SignalIndex <-- [%s]", channel->name);
        log_simbus("    model_uid=%d", model_uid);
        process_signal_index_message(adapter, channel, model_uid,
            ns(ChannelMessage_message(channel_message)));
    }
    /* SignalRead */
    else if (msg_type == ns(MessageType_SignalRead)) {
        log_simbus("SignalRead <-- [%s]", channel->name);
        log_simbus("    model_uid=%d", model_uid);
        process_signal_read_message(adapter, channel, model_uid,
            ns(ChannelMessage_message(channel_message)));
    }
    /* SignalWrite */
    else if (msg_type == ns(MessageType_SignalWrite)) {
        log_simbus("SignalWrite <-- [%s]", channel->name);
        log_simbus("    model_uid=%d", model_uid);
        process_signal_write_message(adapter, channel, model_uid,
            ns(ChannelMessage_message(channel_message)));
    }
    /* ModelReady */
    else if (msg_type == ns(MessageType_ModelReady)) {
        log_simbus("ModelReady <-- [%s]", channel->name);
        log_simbus("    model_uid=%d", model_uid);
        /* Handle ModelReady and embedded SignalWrite message. */
        process_model_ready_message(adapter, channel, model_uid,
            ns(ChannelMessage_message(channel_message)));
        simbus_model_at_ready(am, channel, model_uid);
        /* Resolve the Bus. */
        if (simbus_network_ready(am) && simbus_models_ready(am)) {
            /**
            This condition will exist when the last model on the last channel
            sends its ModelReady message. When that happens, resolve the bus.

            Time on the Bus is progressed according to Bus Cycle Time.

            Increment the bus time via Kahan summation.
            */
            double y = adapter->bus_step_size - adapter->bus_time_correction;
            double t = adapter->bus_time + y;
            adapter->bus_time_correction = (t - adapter->bus_time) - y;
            adapter->bus_time = t;

            double model_time = adapter->bus_time;
            double stop_time = model_time + adapter->bus_step_size;
            if (0) {
                for (uint32_t i = 0; i < am->channels_length; i++) {
                    Channel* ch = _get_channel_byindex(am, i);
                    _refresh_index(ch);
                    resolve_channel_and_model_start(
                        adapter, ch, model_time, stop_time);
                }
            }

            resolve_and_notify(adapter, model_time, stop_time);
            simbus_models_to_start(am);
        }
    }
    /* ModelExit */
    else if (msg_type == ns(MessageType_ModelExit)) {
        log_simbus("ModelExit <-- [%s]", channel->name);
        log_simbus("    model_uid=%d", model_uid);

        simbus_model_at_exit(am, channel, model_uid);
    }
    /* Unexpected message? */
    else {
        log_simbus("UNEXPECTED <-- [%s]", channel->name);
        log_simbus("    model_uid=%d", model_uid);
        log_simbus("    message_type (%d)", msg_type);
        return;
    }
}
