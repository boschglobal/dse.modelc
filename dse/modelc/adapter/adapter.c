// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <msgpack.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/adapter_private.h>
#include <dse/modelc/adapter/message.h>
#include <dse/modelc/controller/model_private.h>


#define UNUSED(x) ((void)x)

#undef ns
#define ns(x) FLATBUFFERS_WRAP_NAMESPACE(dse_schemas_fbs_channel, x)


static void handle_channel_message(Adapter* adapter, const char* channel_name,
    ns(ChannelMessage_table_t) channel_message, int32_t          token);


static void _update_channels_keys(AdapterModel* am)
{
    if (am->channels_keys) {
        for (uint32_t _ = 0; _ < am->channels_length; _++)
            free(am->channels_keys[_]);
        free(am->channels_keys);
        am->channels_keys = NULL;
    }
    am->channels_keys = hashmap_keys(&am->channels);
    am->channels_length = hashmap_number_keys(am->channels);
}


static Channel* _create_channel(AdapterModel* am, const char* channel_name)
{
    assert(am);

    Channel* ch = hashmap_get(&am->channels, channel_name);
    if (ch) {
        assert(ch->name == channel_name);
        return ch;
    }
    /* Create a new Channel object. */
    ch = calloc(1, sizeof(Channel));
    ch->name = channel_name;
    int rc = hashmap_init(&ch->signal_values);
    if (rc) {
        log_error("Hashmap init failed for _create_channel.signal_values!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }
    /* Set the endpoint object. */
    if (am->adapter && am->adapter->endpoint) {
        ch->endpoint_channel = am->adapter->endpoint->create_channel(
            am->adapter->endpoint, ch->name);
    }

    /* Add the new Channel to the hashmap. */
    if (hashmap_set(&am->channels, ch->name, ch)) {
        _update_channels_keys(am);
        return ch;
    }
    log_error("Adapter _create_channel failed to create new Channel object!");
    goto error_clean_up;

error_clean_up:
    free(ch);
    return NULL;
}


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


Channel* adapter_get_channel(AdapterModel* am, const char* channel_name)
{
    /* Public interface to get channel, decoupled. */
    return _get_channel(am, channel_name);
}


Adapter* adapter_create(void* endpoint)
{
    assert(endpoint);

    Adapter* adapter;
    int      rc;

    errno = 0;
    adapter = calloc(1, sizeof(Adapter));
    if (adapter == NULL) {
        log_error("Adapter malloc failed!");
        goto error_clean_up;
    }
    adapter->private = calloc(1, sizeof(AdapterPrivate));
    if (adapter->private == NULL) {
        log_error("adapter->private malloc failed!");
        goto error_clean_up;
    }
    rc = hashmap_init(&adapter->models);
    if (rc) {
        log_error("Hashmap init failed for adapter_create!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }

    /* Initialise supporting properties of the Adapter. */
    adapter->stop_request = false;
    adapter->endpoint = endpoint;
    AdapterPrivate* _ap = (AdapterPrivate*)(adapter->private);
    _ap->handle_message = handle_channel_message;

    flatcc_builder_t* builder = &(_ap->builder);
    flatcc_builder_init(builder);
    builder->buffer_flags |= flatcc_builder_with_size;

    return adapter;

error_clean_up:
    adapter_destroy(adapter);
    return NULL;
}


SignalValue* _get_signal_value(Channel* channel, const char* signal_name)
{
    SignalValue* sv = hashmap_get(&channel->signal_values, signal_name);
    if (sv) return sv;

    /* Add a new SignalValue, assume dynamically provided name. */
    sv = calloc(1, sizeof(SignalValue));
    assert(sv);
    sv->name = malloc(strlen(signal_name) + 1);
    snprintf(sv->name, strlen(signal_name) + 1, "%s", signal_name);
    if (hashmap_set(&channel->signal_values, signal_name, sv)) return sv;
    assert(0); /* Should not happen. */
    return NULL;
}


static SignalMap* _get_signal_value_map(
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


void _update_signal_value_keys(Channel* channel)
{
    if (channel->signal_values_keys) {
        for (uint32_t _ = 0; _ < channel->signal_values_length; _++)
            free(channel->signal_values_keys[_]);
        free(channel->signal_values_keys);
        channel->signal_values_keys = NULL;
    }
    if (channel->signal_values_map) {
        free(channel->signal_values_map);
        channel->signal_values_map = NULL;
    }
    channel->signal_values_keys = hashmap_keys(&channel->signal_values);
    channel->signal_values_length = hashmap_number_keys(channel->signal_values);
    channel->signal_values_map = _get_signal_value_map(channel,
        (const char**)channel->signal_values_keys,
        channel->signal_values_length);
}


SignalValue* _get_signal_value_byindex(Channel* channel, uint32_t index)
{
    assert(index < channel->signal_values_length);
    const char* signal_name = channel->signal_values_keys[index];
    return _get_signal_value(channel, signal_name);
}


SignalMap* adapter_get_signal_map(AdapterModel* am, const char* channel_name,
    const char** signal_name, uint32_t signal_count)
{
    /* Generate a mapping from Index to SignalValue to avoid constant
       hashmap lookups. Generate each time new signals are added.

       This will generate an array of map objects. The indexing will
       match the callers signal_name array, the map holds a pointer
       to the signal value of the adapter (the consolidation point).

       This way, a Model Function can get a _subset_ of all channel signals.

       Caller to free SignalMap, but not referenced SignalValue object.
    */
    SignalMap* sm;
    Channel*   ch = _get_channel(am, channel_name);
    assert(ch);

    sm = _get_signal_value_map(ch, signal_name, signal_count);
    _update_signal_value_keys(ch);
    return sm;
}


Channel* adapter_init_channel(AdapterModel* am, const char* channel_name,
    const char** signal_name, uint32_t signal_count)
{
    assert(am);

    errno = 0;
    Channel* ch = _create_channel(am, channel_name);
    assert(ch);

    /* Initialise the Signal properties. */
    for (uint32_t i = 0; i < signal_count; i++) {
        _get_signal_value(ch, signal_name[i]); /* Creates if missing. */
    }
    _update_signal_value_keys(ch);

    /* Return the channel object so that caller can configure other properties.
     */
    return ch;
}


static void _adapter_connect(
    AdapterModel* am, SimulationSpec* sim, int retry_count)
{
    Adapter*          adapter = am->adapter;
    AdapterPrivate*   _ap = (AdapterPrivate*)(adapter->private);
    flatcc_builder_t* builder = &(_ap->builder);
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
}


void adapter_connect(Adapter* adapter, SimulationSpec* sim, int retry_count)
{
    UNUSED(adapter);

    /* Sequential according to the Model Instances. */

    /* Send all ModelRegister messages. */
    ModelInstanceSpec* _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        ModelInstancePrivate* mip = _instptr->private;
        AdapterModel*         am = mip->adapter_model;
        _adapter_connect(am, sim, retry_count);
        /* Next instance? */
        _instptr++;
    }
}


// Each adapter channel will have a different set of signals, so its a
//  diff message for each.
// The data structures should be configured by the call to adapter_init_channel
// Only need to do the messaging here!!


static void _adapter_register(AdapterModel* am)
{
    Adapter*          adapter = am->adapter;
    AdapterPrivate*   _ap = (AdapterPrivate*)(adapter->private);
    flatcc_builder_t* builder = &(_ap->builder);

    /* SignalIndex on all channels. */
    for (uint32_t channel_index = 0; channel_index < am->channels_length;
         channel_index++) {
        Channel* ch = _get_channel_byindex(am, channel_index);
        ns(MessageType_union_ref_t) message;
        uint32_t signal_list_length = ch->signal_values_length;

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
}


void adapter_register(Adapter* adapter, SimulationSpec* sim)
{
    UNUSED(adapter);

    /* Sequential according to the Model Instances. */

    /* Send all ModelRegister messages. */
    ModelInstanceSpec* _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        ModelInstancePrivate* mip = _instptr->private;
        AdapterModel*         am = mip->adapter_model;
        _adapter_register(am);
        /* Next instance? */
        _instptr++;
    }
}


static int _adapter_model_ready(AdapterModel* am)
{
    Adapter*          adapter = am->adapter;
    AdapterPrivate*   _ap = (AdapterPrivate*)(adapter->private);
    flatcc_builder_t* builder = &(_ap->builder);

    /* ModelReady on all channels. */
    for (uint32_t channel_index = 0; channel_index < am->channels_length;
         channel_index++) {
        Channel* ch = _get_channel_byindex(am, channel_index);
        assert(ch);
        assert(ch->signal_values_map);

        /* ModelReady with SignalWrite */
        log_simbus("ModelReady --> [%s]", ch->name);
        flatcc_builder_reset(builder);

        /* Encode SignalWrite MsgPack payload: data:[ubyte] = [[UID],[Value]] */
        msgpack_sbuffer sbuf;
        msgpack_packer  pk;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
        /* First(root) Object, array, 2 elements. */
        msgpack_pack_array(&pk, 2);
        /* Count changed signals. */
        uint32_t changed_signal_count = 0;
        for (uint32_t i = 0; i < ch->signal_values_length; i++) {
            SignalValue* sv = ch->signal_values_map[i].signal;
            if (sv->uid == 0) continue;
            if ((sv->val != sv->final_val) || (sv->bin && sv->bin_size)) {
                changed_signal_count++;
            }
        }
        /* 1st Object in root Array, list of UID's. */
        msgpack_pack_array(&pk, changed_signal_count);
        for (uint32_t i = 0; i < ch->signal_values_length; i++) {
            SignalValue* sv = ch->signal_values_map[i].signal;
            if (sv->uid == 0) continue;
            if ((sv->val != sv->final_val) || (sv->bin && sv->bin_size)) {
                msgpack_pack_uint32(&pk, sv->uid);
            }
        }
        /* 2st Object in root Array, list of Values. */
        msgpack_pack_array(&pk, changed_signal_count);
        for (uint32_t i = 0; i < ch->signal_values_length; i++) {
            SignalValue* sv = ch->signal_values_map[i].signal;
            if (sv->uid == 0) continue;
            if (sv->bin && sv->bin_size) {
                msgpack_pack_bin_with_body(&pk, sv->bin, sv->bin_size);
                log_simbus("    SignalWrite: %u = <binary> (len=%u) [name=%s]",
                    sv->uid, sv->bin_size, sv->name);
                /* Indicate the binary object was consumed. */
                sv->bin_size = 0;
            } else if (sv->val != sv->final_val) {
                msgpack_pack_double(&pk, sv->final_val);
                log_simbus("    SignalWrite: %u = %f [name=%s]", sv->uid,
                    sv->final_val, sv->name);
            }
        }

        /* SignalWrite */
        ns(SignalWrite_ref_t) signal_write_message;
        flatbuffers_uint8_vec_ref_t data_vector;
        data_vector = flatbuffers_uint8_vec_create(
            builder, (uint8_t*)sbuf.data, sbuf.size);
        signal_write_message = ns(SignalWrite_create(builder, data_vector));

        /* ModelReady */
        ns(MessageType_union_ref_t) model_ready_message;
        model_ready_message = ns(MessageType_as_ModelReady(ns(
            ModelReady_create(builder, am->model_time, signal_write_message))));
        log_simbus("    model_time=%f", am->model_time);
        send_message(adapter, ch->endpoint_channel, am->model_uid,
            model_ready_message, false);
        msgpack_sbuffer_destroy(&sbuf);
    }

    return 0;
}


static int _adapter_model_start(AdapterModel* am)
{
    Adapter*          adapter = am->adapter;

    /* Wait on ModelStart from all channels (and handle SignalValue,
     * ParameterValue) */
    uint32_t start_counter = 0;
    log_debug("adapter_ready: wait on ModelStart ...");
    while (start_counter < am->channels_length) {
        const char* msg_channel_name = NULL;
        bool        found = false;
        int         rc = wait_message(
                    adapter, &msg_channel_name, ns(MessageType_ModelStart), 0, &found);
        if (rc != 0) {
            log_error("wait_message returned %d", rc);

            /* Handle specific error conditions. */
            if (rc == ETIME) return rc; /* TIMEOUT */
        }
        if (found) start_counter++;
    }

    return 0;
}


int adapter_ready(Adapter* adapter, SimulationSpec* sim)
{
    UNUSED(adapter);
    ModelInstanceSpec* _instptr;
    int                rc = 0;

    /* Sequential according to the Model Instances. */

    /* Send all ModelReady messages. */
    _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        ModelInstancePrivate* mip = _instptr->private;
        AdapterModel*         am = mip->adapter_model;
        rc |= _adapter_model_ready(am);
        /* Next instance? */
        _instptr++;
    }

    /* Wait for all ModelStart messages. */
    _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        ModelInstancePrivate* mip = _instptr->private;
        AdapterModel*         am = mip->adapter_model;
        rc |= _adapter_model_start(am);
        /* Next instance? */
        _instptr++;
    }

    return rc;
}


void adapter_exit(Adapter* adapter, SimulationSpec* sim)
{
    AdapterPrivate*   _ap = (AdapterPrivate*)(adapter->private);
    flatcc_builder_t* builder = &(_ap->builder);
    ns(MessageType_union_ref_t) message;

    ModelInstanceSpec* _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        ModelInstancePrivate* mip = _instptr->private;
        AdapterModel*         am = mip->adapter_model;

        for (uint32_t channel_index = 0; channel_index < am->channels_length;
             channel_index++) {
            Channel* ch = _get_channel_byindex(am, channel_index);
            log_simbus("ModelExit --> [%s]", ch->name);
            /* ModelExit */
            flatcc_builder_reset(builder);
            message =
                ns(MessageType_as_ModelExit(ns(ModelExit_create(builder))));
            send_message(
                adapter, ch->endpoint_channel, am->model_uid, message, false);
        }

        /* Next instance? */
        _instptr++;
    }
}


void adapter_interrupt(Adapter* adapter)
{
    if (adapter) adapter->stop_request = true;

    if (adapter && adapter->endpoint) {
        Endpoint* endpoint = adapter->endpoint;
        if (endpoint->interrupt) endpoint->interrupt(endpoint);
    }
}


void adapter_destroy(Adapter* adapter)
{
    if (adapter) {
        hashmap_destroy(&adapter->models);
    }
    if (adapter && adapter->endpoint) {
        Endpoint* endpoint = adapter->endpoint;
        endpoint->disconnect(endpoint);
    }
    if (adapter && adapter->bus_adapter_model) {
        adapter_destroy_adapter_model(adapter->bus_adapter_model);
    }
    if (adapter && adapter->private) {
        AdapterPrivate* _ap = (AdapterPrivate*)(adapter->private);

        flatcc_builder_t* builder = &(_ap->builder);
        flatcc_builder_clear(builder);
        free(_ap->ep_buffer);
        free(adapter->private);
    }
    free(adapter);
}


void adapter_destroy_adapter_model(AdapterModel* am)
{
    if (am && am->channels_length) {
        for (uint32_t i = 0; i < am->channels_length; i++) {
            Channel* ch = _get_channel_byindex(am, i);
            for (uint32_t si = 0; si < ch->signal_values_length; si++) {
                SignalValue* sv = _get_signal_value_byindex(ch, si);
                if (sv->name) free(sv->name);
                if (sv->bin) free(sv->bin);
                if (sv) free(sv);
            }
            hashmap_destroy(&ch->signal_values);
            if (ch->signal_values_keys) {
                for (uint32_t _ = 0; _ < ch->signal_values_length; _++)
                    free(ch->signal_values_keys[_]);
            }
            free(ch->signal_values_keys);
            free(ch->signal_values_map);
            if (ch->model_register_set) {
                set_destroy(ch->model_register_set);
                free(ch->model_register_set);
            }
            if (ch->model_ready_set) {
                set_destroy(ch->model_ready_set);
                free(ch->model_ready_set);
            }
            free(ch);
        }
        hashmap_destroy(&am->channels);
        if (am->channels_keys) {
            for (uint32_t _ = 0; _ < am->channels_length; _++)
                free(am->channels_keys[_]);
        }
        free(am->channels_keys);
        am->channels_length = 0;
    }
    free(am);
}


SignalValue* _find_signal_by_uid(Channel* channel, uint32_t uid)
{
    if (uid == 0) return NULL;

    for (unsigned int i = 0; i < channel->signal_values_length; i++) {
        SignalValue* sv = channel->signal_values_map[i].signal;
        if (sv->uid == uid) return sv;
    }
    return NULL;
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


static void process_signal_index_message(
    Channel* channel, ns(SignalIndex_table_t) signal_index_table)
{
    if (!ns(SignalIndex_indexes_is_present(signal_index_table))) {
        log_simbus("WARNING: no indexes in SignalIndex message!");
        return;
    }
    /* Decode the SignalLookup vector. */
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
        for (unsigned int i = 0; i < channel->signal_values_length; i++) {
            SignalValue* sv = channel->signal_values_map[i].signal;
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
    ns(ChannelMessage_table_t) channel_message, int32_t token)
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


void adapter_model_dump_debug(AdapterModel* am, const char* name)
{
    log_simbus("Model Instance : %s", name);
    log_simbus("----------------");
    log_simbus("model_uid      : %u", am->model_uid);
    log_simbus("model_time     : %f", am->model_time);
    log_simbus("stop_time      : %f", am->stop_time);
    log_simbus("channel_count  : %u", am->channels_length);
    log_simbus("----------------");
    log_simbus("Channel Objects:");
    log_simbus("----------------");
    for (uint32_t channel_index = 0; channel_index < am->channels_length;
         channel_index++) {
        Channel* ch = _get_channel_byindex(am, channel_index);
        log_simbus("----------------------------------------");
        log_simbus("Channel [%u]:", channel_index);
        log_simbus("  name         : %s", ch->name);
        log_simbus("  signal_count : %u", ch->signal_values_length);
        log_simbus("  signal_value :");
        for (uint32_t i = 0; i < ch->signal_values_length; i++) {
            SignalValue* sv = ch->signal_values_map[i].signal;
            log_simbus("    [%u] uid=%u, val=%f, final_val=%f, name=%s", i,
                sv->uid, sv->val, sv->final_val, sv->name);
        }
    }
}


void adapter_dump_debug(Adapter* adapter, SimulationSpec* sim)
{
    assert(adapter);

    /* Dump Adapter properties. */
    log_simbus("========================================");
    log_simbus("Adapter Dump");
    log_simbus("========================================");

    log_simbus("Adapter Objects:");
    log_simbus("----------------------------------------");

    ModelInstanceSpec* _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        ModelInstancePrivate* mip = _instptr->private;
        AdapterModel*         am = mip->adapter_model;
        adapter_model_dump_debug(am, _instptr->name);
        /* Next instance? */
        _instptr++;
    }
    log_simbus("========================================");
}
