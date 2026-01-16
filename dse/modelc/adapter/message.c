// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/private.h>
#include <dse/modelc/adapter/message.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse_schemas/flatbuffers/simbus_channel_builder.h>


#define UNUSED(x) ((void)x)


static bool process_sbno_message(
    Adapter* adapter, uint8_t* msg_ptr, int32_t token)
{
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    if (v->handle_notify_message == NULL) {
        log_fatal("No notify message handler for adapter!");
        return false;
    }

    notify(NotifyMessage_table_t) notify_message;
    notify_message = notify(NotifyMessage_as_root(msg_ptr));

    /* ModelRegiser ACK. */
    if (notify(NotifyMessage_model_register_is_present(notify_message))) {
        if (adapter->bus_mode) {
            /* SimBus */
            v->handle_notify_message(adapter, notify_message);
            return true;
        }
        /* Model */
        if (token != 0) {
            /* Model called, waiting for a MODEL_REGISTER ACK. */
            /* NotifyMessage container message.*/
            int32_t notify_token = notify(NotifyMessage_token(notify_message));

            /* ModelRegister table. */
            notify(ModelRegister_table_t) t =
                notify(NotifyMessage_model_register(notify_message));
            uint32_t model_uid = notify(ModelRegister_model_uid(t));

            /* Locate the Adapter Model. */
            char hash_key[UID_KEY_LEN];
            snprintf(hash_key, UID_KEY_LEN - 1, "%d", model_uid);
            AdapterModel* am = hashmap_get(&adapter->models, hash_key);
            if (am == NULL) return false; /* Discard, not for this model. */

            /* Check. */
            if (am->model_uid == model_uid && notify_token == token) {
                /* Don't process the message, ACK only. */
                return true;
            }
        }
        /* No more message processing. */
        return false;
    }

    /* SignalIndex ACK. */
    if (notify(NotifyMessage_signal_index_is_present(notify_message))) {
        if (adapter->bus_mode) {
            /* SimBus */
            v->handle_notify_message(adapter, notify_message);
            return true;
        }
        /* Model */
        if (token != 0) {
            // waiting for a SignalIndex, check the model_uid too.
            /* NotifyMessage container message.*/
            int32_t notify_token = notify(NotifyMessage_token(notify_message));
            /* NotifyMessage container message.*/
            flatbuffers_uint32_vec_t vector =
                notify(NotifyMessage_model_uid(notify_message));
            size_t vector_len = flatbuffers_uint32_vec_len(vector);
            assert(vector_len == 1);
            uint32_t model_uid = flatbuffers_uint32_vec_at(vector, 0);

            /* Locate the channel objects. */
            char hash_key[UID_KEY_LEN];
            snprintf(hash_key, UID_KEY_LEN - 1, "%d", model_uid);
            AdapterModel* am = hashmap_get(&adapter->models, hash_key);
            if (am == NULL) {
                log_trace("Discard index, no model.");
                return false; /* Discard, not for this model. */
            }

            if (am->model_uid == model_uid && notify_token == token) {
                /* Process the message to extract/update indexes. */
                v->handle_notify_message(adapter, notify_message);
                return true;
            }
        }
        /* No more message processing. */
        return false;
    }

    /* ModelExit. */
    if (notify(NotifyMessage_model_exit_is_present(notify_message))) {
        if (adapter->bus_mode) {
            /* SimBus */
            v->handle_notify_message(adapter, notify_message);
            return true;
        }
        /* Model */
        /* No more message processing. */
        return false;
    }

    /* Process the notify message. */
    if (token == 0) {
        v->handle_notify_message(adapter, notify_message);
        return true;
    } else {
        /* Waiting for a message/token. */
    }

    return false;
}


static bool process_message_stream(Adapter* adapter, const char* channel_name,
    uint8_t* buffer, size_t length, int32_t token)
{
    UNUSED(channel_name);

    bool     found = false;
    size_t   msg_len = 0;
    uint8_t* msg_ptr = buffer;

    /* Messages are in a stream, each with a size prefix. */
    while (((msg_ptr - buffer) + msg_len) < (uint32_t)length) {
        uint8_t* raw_msg_ptr = msg_ptr;
        msg_ptr = flatbuffers_read_size_prefix(msg_ptr, &msg_len);
        if (msg_len == 0) {
            log_debug("process_message_stream: size_prefix is 0!?!");
            break;
        };
        /* Trace the incoming message, before processing. */
        if (adapter->trace) {
            fwrite(raw_msg_ptr, sizeof(raw_msg_ptr[0]),
                msg_len + sizeof(flatbuffers_uoffset_t), adapter->trace);
        }
        /* Process the message. */
        if (flatbuffers_has_identifier(msg_ptr, "SBNO")) {
            found |= process_sbno_message(adapter, msg_ptr, token);
        } else {
            log_debug("process_message_stream: identifier unknown!?!");
            break;
        }
        /* Next. */
        msg_ptr += msg_len;
    }

    return found;
}


int32_t wait_message(
    Adapter* adapter, const char** channel_name, int32_t token, bool* found)
{
    assert(adapter);
    assert(adapter->vtable);
    assert(found);

    Endpoint*         endpoint = adapter->endpoint;
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    uint8_t**         buffer = &(v->ep_buffer); /* realloc may be called */
    uint32_t*         buffer_length = &(v->ep_buffer_length);
    int32_t           length = v->ep_buffer_length;

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
            adapter, msg_channel_name, *buffer, length, token);
        /* Condition: found (message_type or token). */
        if (*found) {
            if (channel_name) *channel_name = msg_channel_name;
            break;
        }
        /* Condition: bus mode
           Message processed, caller determines next action. */
        if (adapter->bus_mode) break;
    }

    return 0;
}


int32_t send_notify_message(
    Adapter* adapter, uint32_t model_uid, notify(NotifyMessage_ref_t) message)
{
    Endpoint*         endpoint = adapter->endpoint;
    AdapterMsgVTable* v = (AdapterMsgVTable*)adapter->vtable;
    flatcc_builder_t* B = &(v->builder);

    flatcc_builder_create_buffer(B, flatbuffers_notify_identifier,
        B->block_align, message, B->min_align, B->buffer_flags);
    size_t   size = 0;
    uint8_t* buf = flatcc_builder_finalize_buffer(B, &size);

    /* Trace. */
    if (adapter->trace) {
        fwrite(buf, sizeof(buf[0]), size, adapter->trace);
    }

    /* Send the Channel Message with the configured Transport. */
    endpoint->send_fbs(endpoint, NULL, buf, (uint32_t)size, model_uid);
    FLATCC_BUILDER_FREE(buf);
    return 0;
}


int32_t send_notify_message_wait_ack(Adapter* adapter, uint32_t model_uid,
    notify(NotifyMessage_ref_t) message, int32_t                token)
{
    int32_t rc = send_notify_message(adapter, model_uid, message);
    if (rc != 0) return rc;
    if (token == 0) return 0;

    /* Wait for a Notify message with the same Token (and model_uid). */
    bool found = false;
    rc = wait_message(adapter, NULL, token, &found);
    if (rc != 0) {
        log_error("wait_message returned %d", rc);
    }
    if (found && adapter->bus_mode) {
        // Should not happen, interested if it does.
        assert(0);
    }
    if (found == false) {
        if (rc == ETIME) {
            rc = ETIME; /* TIMEOUT */
        } else {
            rc = ENOMSG;
            log_error("WARNING: no ACK received!");
        }
    }

    return rc;
}
