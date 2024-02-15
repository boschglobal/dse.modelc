// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <msgpack.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/transport/endpoint.h>


msgpack_sbuffer mp_encode_fbs(
    void* buffer, uint32_t buffer_length, const char* channel_name)
{
    /**
     * Encode as a MsgPack datagram:
     *      MSG:Object[0]: message indicator (SBNO or SBCH) (string)
     *      MSG:Object[1]: channel name (string)
     *      MSG:Object[2]: buffer (FBS) (bin)
     */
    msgpack_sbuffer sbuf;
    msgpack_packer  pk;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
    if (channel_name) {
        /* Sending a Channel Message. */
        int _ch_name_len = strlen(channel_name);
        msgpack_pack_str(&pk, 4);
        msgpack_pack_str_body(&pk, "SBCH", 4);
        msgpack_pack_str(&pk, _ch_name_len);
        msgpack_pack_str_body(&pk, channel_name, _ch_name_len);
    } else {
        /* Sending a Notify Message. */
        msgpack_pack_str(&pk, 4);
        msgpack_pack_str_body(&pk, "SBNO", 4);
        msgpack_pack_str(&pk, 0);
        msgpack_pack_str_body(&pk, "", 0);
    }
    msgpack_pack_bin(&pk, buffer_length);
    msgpack_pack_bin_body(&pk, (uint8_t*)buffer, buffer_length);

    /* Caller must call: msgpack_sbuffer_destroy(&sbuf) */
    return sbuf;
}


int32_t mp_decode_fbs(char* msg, int msg_len, uint8_t** buffer,
    uint32_t* buffer_length, Endpoint* endpoint, const char** channel_name)
{
    int return_len = 0;

    /**
     * Decode as a MsgPack datagram:
     *      MSG:Object[0]: message indicator (SBNO or SBCH) (string)
     *      MSG:Object[1]: channel name (string)
     *      MSG:Object[2]: buffer (FBS) (bin)
     */
    bool             result;
    msgpack_unpacker unpacker;
    // +4 is COUNTER_SIZE.
    result = msgpack_unpacker_init(&unpacker, msg_len + 4);
    if (!result) {
        log_error("MsgPack unpacker init failed!");
        goto error_clean_up;
    }
    if (msgpack_unpacker_buffer_capacity(&unpacker) < (size_t)msg_len) {
        log_error("MsgPack unpacker buffer size too small!");
        goto error_clean_up;
    }
    memcpy(msgpack_unpacker_buffer(&unpacker), (const char*)msg, msg_len);
    msgpack_unpacker_buffer_consumed(&unpacker, msg_len);

    msgpack_unpacked      unpacked;
    msgpack_unpack_return ret;
    msgpack_unpacked_init(&unpacked);
    msgpack_object obj;
    /* Object[0]: message indicator (string) */
    ret = msgpack_unpacker_next(&unpacker, &unpacked);
    if (ret != MSGPACK_UNPACK_SUCCESS) {
        log_simbus("WARNING: data vector unpacked with unexpected return code! "
                   "(ret=%d)",
            ret);
        log_error("MsgPack msgpack_unpacker_next failed!");
        goto error_clean_up;
    }
    obj = unpacked.data;
    assert(obj.type == MSGPACK_OBJECT_STR);
    static char msg_ind[10];
    assert(obj.via.str.size < 10);
    strncpy(msg_ind, obj.via.str.ptr, obj.via.str.size);
    msg_ind[obj.via.str.size + 1] = '\0';
    /* Object[1]: channel name (string) */
    ret = msgpack_unpacker_next(&unpacker, &unpacked);
    if (ret != MSGPACK_UNPACK_SUCCESS) {
        log_simbus("WARNING: data vector unpacked with unexpected return code! "
                   "(ret=%d)",
            ret);
        log_error("MsgPack msgpack_unpacker_next failed!");
        goto error_clean_up;
    }
    if (strcmp(msg_ind, "SBCH") == 0) {
        obj = unpacked.data;
        assert(obj.type == MSGPACK_OBJECT_STR);
        static char ch_name[100];
        assert(obj.via.str.size < 100);
        strncpy(ch_name, obj.via.str.ptr, obj.via.str.size);
        ch_name[obj.via.str.size + 1] = '\0';
        const char* _ = hashmap_get(&endpoint->endpoint_channels, ch_name);
        assert(_);         /* Channel name should be known. */
        *channel_name = _; /* Return the const char* version of ch_name. */
    } else {
        /* Potential Notify message. */
    }
    /* Object[2]: buffer (FBS) (bin) */
    ret = msgpack_unpacker_next(&unpacker, &unpacked);
    if (ret != MSGPACK_UNPACK_SUCCESS) {
        log_simbus("WARNING: data vector unpacked with unexpected return code! "
                   "(ret=%d)",
            ret);
        log_error("MsgPack msgpack_unpacker_next failed!");
        goto error_clean_up;
    }
    obj = unpacked.data;
    assert(obj.type == MSGPACK_OBJECT_BIN);
    const char* bin_ptr = obj.via.bin.ptr;
    uint32_t    bin_size = obj.via.bin.size;
    /* Marshal data to caller. */
    if (bin_size > *buffer_length) {
        /* Prepare the buffer, resize if necessary. */
        *buffer = realloc(*buffer, bin_size);
        if (*buffer == NULL) {
            log_error("Malloc failed!");
        }
        *buffer_length = (size_t)bin_size;
    }
    memset(*buffer, 0, *buffer_length);
    memcpy(*buffer, bin_ptr, bin_size);
    return_len = bin_size;

    /* Release any allocated objects. */
error_clean_up:
    /* Unpack: Cleanup. */
    msgpack_unpacked_destroy(&unpacked);
    msgpack_unpacker_destroy(&unpacker);

    /* Return the buffer length (+ve) as indicator of success. */
    return (uint32_t)return_len;
}
