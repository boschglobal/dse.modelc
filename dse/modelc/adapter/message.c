// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdbool.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/adapter_private.h>
#include <dse/modelc/adapter/message.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse_schemas/flatbuffers/simbus_channel_builder.h>



#undef ns
#define ns(x) FLATBUFFERS_WRAP_NAMESPACE(dse_schemas_fbs_channel, x)


/**
The operation of this API is typically:

    configure_handler();
    ready_count = 0;
    send_message(ready);
    wait_message(ready_response);
        for each message()
            message_handle();
                if ready_response then increment ready_count;
            found = true;
        if found return true;
    if ready_count then
        send_message(start)

So, the handler should extract/maintain any state needed, and the main loop can
decide if/what message should be sent next based on that state.
 */


static int32_t get_token(void)
{
    static int32_t _token = 0;
    return ++_token;
    // TODO: use an random int or handle overflow.
}


static bool process_message_stream(Adapter* adapter, const char* channel_name,
    uint8_t* buffer, size_t length, ns(MessageType_union_type_t) message_type,
    int32_t token)
{
    AdapterPrivate* _ap = (AdapterPrivate*)(adapter->private);
    bool            ack_found = false;
    bool            msg_type_found = false;
    size_t          msg_len = 0;
    uint8_t*        msg_ptr = buffer;

    /* Messages are in a stream, each with a size prefix. */
    while (((msg_ptr - buffer) + msg_len) < (uint32_t)length) {
        msg_ptr = flatbuffers_read_size_prefix(msg_ptr, &msg_len);
        if (msg_len == 0) break;
        if (!flatbuffers_has_identifier(msg_ptr, "SBCH")) break;

        /* Extract the Channel Message and Token. */
        ns(ChannelMessage_table_t) channel_message;
        int32_t  message_token = 0;
        uint32_t message_model_uid = 0;
        channel_message = ns(ChannelMessage_as_root(msg_ptr));
        if (ns(ChannelMessage_token_is_present(channel_message)))
            message_token = ns(ChannelMessage_token(channel_message));
        if (ns(ChannelMessage_model_uid_is_present(channel_message)))
            message_model_uid = ns(ChannelMessage_model_uid(channel_message));
        char message_model_uid_key[UID_KEY_LEN];
        snprintf(
            message_model_uid_key, UID_KEY_LEN - 1, "%d", message_model_uid);
        bool uid_match = false;
        if (hashmap_get(&adapter->models, message_model_uid_key)) {
            uid_match = true;
        }

        /* Is this the wait ACK Message? */
        if (uid_match && token) {
            log_trace("    token = %d (waiting for %d)", message_token, token);
            if (token == message_token) {
                ack_found = true;
                log_trace("    msg ack (token=%d)", message_token);
            }
        }
        /* Call the Message Handler. */
        if (adapter->bus_mode || (uid_match && !ack_found)) {
            ns(MessageType_union_type_t) _msg_type;
            _msg_type = ns(ChannelMessage_message_type(channel_message));
            if (_msg_type != ns(MessageType_NONE)) {
                /* If the caller is a Bus Adapter,
                   message_type==ns(MessageType_NONE) and the handler_message()
                   _should_ be called (as expected).
                 */
                if (_msg_type == message_type) {
                    msg_type_found = true;
                }
                _ap->handle_message(
                    adapter, channel_name, channel_message, message_token);
            }
        }

        /* Next. */
        msg_ptr += msg_len;
    }

    return (ack_found | msg_type_found);
}


int32_t wait_message(Adapter* adapter, const char**    channel_name,
    ns(MessageType_union_type_t) message_type, int32_t token, bool* found)
{
    assert(adapter);
    assert(adapter->private);
    assert(found);
    AdapterPrivate* _ap = (AdapterPrivate*)(adapter->private);
    Endpoint*       endpoint = adapter->endpoint;
    uint8_t**       buffer = &(_ap->ep_buffer); /* realloc may be called */
    uint32_t*       buffer_length = &(_ap->ep_buffer_length);
    int32_t         length = _ap->ep_buffer_length;

    *found = false;

    while (1) {
        /* Receive a FBS Message Stream. */
        const char* msg_channel_name = NULL;

        errno = 0;
        length = endpoint->recv_fbs(
            endpoint, &msg_channel_name, buffer, buffer_length);
        if (length <= 0) {
            /* If the length was 0 (or less) then no message was received. */

            /* Condition: Timeout. */
            if ((errno == ETIME) && (!adapter->bus_mode)) {
                /* (Model Only!)
                   With the current implementation a timeout is a fatal
                   condition, however log the error only so that calling code
                   can attempt a cleanup. A timeout likely indicates that
                   another model has exited the simulation. Eventually an Agent
                   (Redis SimBus ) will handle that situation cleanly and this
                   particular timeout condition would be avoided.
                */
                log_error("Timeout while waiting for a message!");
                return ETIME;
            }

            /* Condition: any other conditions.
               Break, found remains false, return 0.
               Caller determines next action. */
            break;
        }
        /* Process the FBS Message Stream. */
        *found = process_message_stream(
            adapter, msg_channel_name, *buffer, length, message_type, token);
        /* Condition: found (message_type or token). */
        if (*found) {
            *channel_name = msg_channel_name;
            break;
        }
    }
    return 0;
}


/**send_message

NOTE: the message will already be encoded to the builder object on the
adapter->private, therefore, this function should only be called once per
message (i.e. send_all should be unwound to a for loop with a
flacc_builder_reset() call on each loop ... or a buffer memcpy).
 */
int32_t send_message(Adapter* adapter, void* endpoint_channel,
    uint32_t model_uid, ns(MessageType_union_ref_t) message, bool ack)
{
    //    assert(am);
    //    Adapter *adapter = am->adapter;
    assert(model_uid);
    assert(adapter);
    assert(adapter->private);
    AdapterPrivate*   _ap = (AdapterPrivate*)(adapter->private);
    Endpoint*         endpoint = adapter->endpoint;
    flatcc_builder_t* builder = &(_ap->builder);
    uint8_t*          buf;
    size_t            size;
    int32_t           token = 0;
    int               rc;
    ns(ChannelMessage_ref_t) channel_message;
    int send_message__rc = 0;

    //    if (model_uid == 0) model_uid = adapter->model_uid;
    if (ack) token = get_token();
    /* Build the Channel Message (without 'create' because it wants all
     * parameters) */
    ns(ChannelMessage_start)(builder);
    ns(ChannelMessage_model_uid_add)(builder, model_uid);
    ns(ChannelMessage_message_add_value)(builder, message);
    if (ack) ns(ChannelMessage_token_add)(builder, token);
    ns(ChannelMessage_message_add_type)(builder, message.type);
    channel_message = ns(ChannelMessage_end)(builder);
    /* Construct the buffer. */
    flatcc_builder_create_buffer(builder, flatbuffers_identifier,
        builder->block_align, channel_message, builder->min_align,
        builder->buffer_flags);
    buf = flatcc_builder_finalize_buffer(
        builder, &size); /* malloc, must call free. */

    /* Send the Channel Message with the configured Transport. */
    rc = endpoint->send_fbs(
        endpoint, endpoint_channel, buf, (uint32_t)size, model_uid);
    if (rc != 0) {
        send_message__rc = EBADMSG;
        log_error("send_fbs returned %d", rc);
        goto error_clean_up;
    }
    if (token > 0) {
        const char* _msg_channel_name = NULL;
        bool        found = false;
        rc = wait_message(
            adapter, &_msg_channel_name, ns(MessageType_NONE), token, &found);
        if (rc != 0) {
            log_error("wait_message returned %d", rc);
        }
        if (found && adapter->bus_mode) {
            // Should not happen, interested if it does.
            assert(_msg_channel_name == NULL);
        }
        if (found == false) {
            if (rc == ETIME) {
                send_message__rc = ETIME; /* TIMEOUT */
            } else {
                send_message__rc = ENOMSG;
                log_error("WARNING: no ACK received!");
            }
        }
    }

    /* Echo the outgoing messages for development/debug. */
    if (0) {
        size_t   msg_len = 0;
        uint8_t* msg_ptr = buf;
        msg_ptr = flatbuffers_read_size_prefix(msg_ptr, &msg_len);
        ns(ChannelMessage_table_t) channel_message;
        channel_message = ns(ChannelMessage_as_root(msg_ptr));
        _ap->handle_message(adapter, 0, channel_message, 0);
    }

error_clean_up:
    FLATCC_BUILDER_FREE(buf);
    return send_message__rc;
}


/**send_message_ack

NOTE: when calling send_message_ack() start a new builder object and make a
minimal copy of the message to be acked (i.e. default values).
 */
int32_t send_message_ack(Adapter* adapter, void* endpoint_channel,
    uint32_t model_uid, ns(MessageType_union_ref_t) message, int32_t token,
    int32_t rc, char* response)
{
    assert(adapter);
    assert(adapter->private);
    assert(token || rc);

    AdapterPrivate*   _ap = (AdapterPrivate*)(adapter->private);
    Endpoint*         endpoint = adapter->endpoint;
    flatcc_builder_t* builder = &(_ap->builder);
    uint8_t*          buf;
    size_t            size;
    ns(ChannelMessage_ref_t) channel_message;

    /* Build the Channel Message (without 'create' because it wants all
     * parameters) */
    ns(ChannelMessage_start)(builder);
    ns(ChannelMessage_model_uid_add)(builder, model_uid);
    ns(ChannelMessage_message_add_value)(builder, message);
    ns(ChannelMessage_message_add_type)(builder, message.type);
    ns(ChannelMessage_token_add)(builder, token);
    ns(ChannelMessage_rc_add)(builder, rc);
    if (response) {
        flatbuffers_string_ref_t _response;
        _response = flatbuffers_string_create_str(builder, response);
        ns(ChannelMessage_response_add)(builder, _response);
    }
    channel_message = ns(ChannelMessage_end)(builder);
    /* Construct the buffer. */
    flatcc_builder_create_buffer(builder, flatbuffers_identifier,
        builder->block_align, channel_message, builder->min_align,
        builder->buffer_flags);
    buf = flatcc_builder_finalize_buffer(
        builder, &size); /* malloc, must call free. */

    /* Send the Channel Message with the configured Transport. */
    rc = endpoint->send_fbs(
        endpoint, endpoint_channel, buf, (uint32_t)size, model_uid);
    if (rc != 0) goto error_clean_up;

    /* Echo the outgoing messages for development/debug. */
    if (0) {
        size_t   msg_len = 0;
        uint8_t* msg_ptr = buf;
        msg_ptr = flatbuffers_read_size_prefix(msg_ptr, &msg_len);
        ns(ChannelMessage_table_t) channel_message;
        channel_message = ns(ChannelMessage_as_root(msg_ptr));
        _ap->handle_message(adapter, 0, channel_message, 0);
    }

error_clean_up:
    FLATCC_BUILDER_FREE(buf);
    return 0;
}
