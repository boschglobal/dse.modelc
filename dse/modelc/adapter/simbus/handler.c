// Copyright 2023, 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

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
    flatcc_builder_t* B = &(v->builder);

    flatcc_builder_reset(B);

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
        resp__signal_name = flatbuffers_string_create_str(B, signal_name);
        ns(SignalLookup_start(B));
        ns(SignalLookup_name_add(B, resp__signal_name));
        ns(SignalLookup_signal_uid_add(B, signal_uid));
        resp__signal_lookup_list[_vi] = ns(SignalLookup_end(B));
    }
    _generate_index(channel);

    /* Create the response Lookup vecotr. */
    log_simbus("SignalIndex --> [%s]", channel->name);
    ns(SignalLookup_vec_ref_t) resp__signal_lookup_vector;
    ns(SignalLookup_vec_start(B));
    for (uint32_t i = 0; i < vector_len; i++)
        ns(SignalLookup_vec_push(B, resp__signal_lookup_list[i]));
    resp__signal_lookup_vector = ns(SignalLookup_vec_end(B));
    /* Construct the response SignalIndex message. */
    ns(MessageType_union_ref_t) resp__message;
    resp__message = ns(MessageType_as_SignalIndex(
        ns(SignalIndex_create(B, resp__signal_lookup_vector))));
    send_message(
        adapter, channel->endpoint_channel, model_uid, resp__message, false);
    free(resp__signal_lookup_list);
}


static void process_notify_signalvector(Adapter* adapter, Channel* channel,
    uint32_t model_uid, notify(SignalVector_table_t) signal_vector)
{
    UNUSED(adapter);
    UNUSED(model_uid);

    /* Process vector encoded binary signal data. */
    notify(BinarySignal_vec_t) binary_signal_vec =
        notify(SignalVector_binary_signal(signal_vector));
    size_t binary_signal_vec_len =
        notify(BinarySignal_vec_len)(binary_signal_vec);
    for (size_t i = 0; i < binary_signal_vec_len; i++) {
        notify(BinarySignal_table_t) binary_signal =
            notify(BinarySignal_vec_at(binary_signal_vec, i));
        uint32_t                _uid = notify(BinarySignal_uid(binary_signal));
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
        notify(Signal_struct_t) signal = notify(Signal_vec_at(signal_vec, i));
        uint32_t _uid = signal->uid;
        double   _value = signal->value;

        SignalValue* sv = _find_signal_by_uid(channel, _uid);
        if ((sv == NULL) && _uid) {
            log_simbus("WARNING: signal with uid (%u) not found!", _uid);
            continue;
        }
        /* Reset final_val (changes will trigger SignalWrite) */
        sv->final_val = _value;
        log_simbus("    SignalWrite: %u = %f [name=%s, prev=%f]", _uid,
            sv->final_val, sv->name, sv->val);
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
    flatcc_builder_t* B = &(v->builder);

    flatcc_builder_reset(B);

    log_simbus("Notify/ModelStart --> [...]");
    log_simbus("    model_time=%f", model_time);
    log_simbus("    schedule_time=%f", schedule_time);

    /* SignalVector vector. */
    notify(SignalVector_vec_start(B));
    for (uint32_t i = 0; i < am->channels_length; i++) {
        Channel* ch = _get_channel_byindex(am, i);
        _refresh_index(ch);
        log_simbus("SignalVector --> [%s]", ch->name);

        /* SignalVector table. */
        notify(SignalVector_start(B));
        notify(SignalVector_name_add(
            B, flatbuffers_string_create_str(B, ch->name)));
        notify(SignalVector_model_uid_add(B, am->model_uid));

        /* Signal vector. */
        size_t binary_signal_count = 0;
        notify(SignalVector_signal_start(B));
        for (uint32_t i = 0; i < ch->index.count; i++) {
            SignalValue* sv = ch->index.map[i].signal;
            if ((sv->val != sv->final_val) && sv->uid) {
                notify(
                    SignalVector_signal_push_create(B, sv->uid, sv->final_val));
                log_simbus("    SignalValue: %u = %f [name=%s]", sv->uid,
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
            for (uint32_t i = 0; i < ch->index.count; i++) {
                SignalValue* sv = ch->index.map[i].signal;
                if (sv->bin && sv->bin_size && sv->uid) {
                    flatbuffers_uint8_vec_ref_t data =
                        flatbuffers_uint8_vec_create(
                            B, (uint8_t*)sv->bin, sv->bin_size);
                    notify(SignalVector_binary_signal_push_create(
                        B, sv->uid, data));
                    log_simbus(
                        "    SignalValue: %u = <binary> (len=%u) [name=%s]",
                        sv->uid, sv->bin_size, sv->name);
                }
            }
            notify(SignalVector_binary_signal_add(
                B, notify(SignalVector_binary_signal_end(B))));
        }
        notify(SignalVector_vec_push(B, notify(SignalVector_end(B))));

        /* Resolve the channel. */
        resolve_channel(ch);
    }

    /* NotifyMessage message. */
    notify(SignalVector_vec_ref_t) signals = notify(SignalVector_vec_end(B));
    notify(NotifyMessage_start(B));
    notify(NotifyMessage_signals_add(B, signals));
    notify(NotifyMessage_model_time_add(B, model_time));
    notify(NotifyMessage_schedule_time_add(B, schedule_time));
    notify(NotifyMessage_ref_t) message = notify(NotifyMessage_end(B));
    send_notify_message(adapter, message);
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
    flatcc_builder_t* B = &(v->builder);
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
        log_simbus("    step_size=%f", ns(ModelRegister_step_size(t)));
        log_simbus("    model_uid=%d", model_uid);
        log_simbus("    notify_uid=%d", ns(ModelRegister_notify_uid(t)));
        log_simbus("    token=%d", token);

        Endpoint* endpoint = adapter->endpoint;
        if (endpoint && endpoint->register_notify_uid) {
            uint32_t notify_uid = ns(ModelRegister_notify_uid(t));
            if (notify_uid) {
                endpoint->register_notify_uid(endpoint, notify_uid);
            }
        }

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
            flatcc_builder_reset(B);
            ns(ModelRegister_start)(B);
            ack_message =
                ns(MessageType_as_ModelRegister(ns(ModelRegister_end)(B)));
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
