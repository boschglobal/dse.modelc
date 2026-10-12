// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <string.h>
#include <dse/clib/collections/vector.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/codec/ab/codec.h>


typedef struct CodecHooks {
    NCODEC*     instance;
    NCodecRead  read;
    NCodecWrite write;
    NCodecClose close;
} CodecHooks;

static Vector codec_hooks = {
    .item_size = sizeof(CodecHooks),
    .initial_capacity = VECTOR_DEFAULT_CAPACITY,
};


__attribute__((destructor)) static void custom_codec_unload(void)
{
    vector_reset(&codec_hooks);
}


static CodecHooks* find_hooks(NCODEC* nc)
{
    for (size_t index = 0; index < vector_len(&codec_hooks); index++) {
        CodecHooks* hooks = vector_at(&codec_hooks, index, NULL);
        if (hooks->instance == nc) return hooks;
    }
    return NULL;
}


static int32_t custom_pdu_read(NCODEC* nc, NCodecMessage* msg)
{
    if (nc == NULL) return -EINVAL;
    CodecHooks* hooks = find_hooks(nc);
    if (hooks == NULL) return -EINVAL;

    printf("Custom DLL: read\n");
    return hooks->read(nc, msg);
}


static int32_t custom_pdu_write(NCODEC* nc, NCodecMessage* msg)
{
    if (nc == NULL) return -EINVAL;
    CodecHooks* hooks = find_hooks(nc);
    if (hooks == NULL) return -EINVAL;

    printf("Custom DLL: write\n");
    return hooks->write(nc, msg);
}


static void custom_close(NCODEC* nc)
{
    if (nc == NULL) return;

    for (size_t index = 0; index < vector_len(&codec_hooks); index++) {
        CodecHooks* hooks = vector_at(&codec_hooks, index, NULL);
        if (hooks->instance != nc) continue;

        NCodecClose original_close = hooks->close;
        CodecHooks  last;
        vector_pop(&codec_hooks, &last);
        if (index < vector_len(&codec_hooks)) {
            *hooks = last;
        }
        original_close(nc);
        return;
    }
}


DLL_PUBLIC NCODEC* ncodec_create(const char* mime_type)
{
    NCODEC* nc = ab_codec_create(mime_type);
    if (nc == NULL) return NULL;

    /* Search for the mime_type 'type' parameter. */
    const char* nc_type = NULL;
    for (int i = 0; i >= 0; i++) {
        NCodecConfigItem nci = ncodec_stat(nc, &i);
        if (nci.name == NULL) break;
        if (strcmp(nci.name, "type") == 0) {
            nc_type = nci.value;
            break;
        }
    }

    /* Install the custom methods. */
    if (nc_type != NULL && strcmp(nc_type, "pdu") == 0) {
        NCodecInstance* instance = (NCodecInstance*)nc;
        CodecHooks      hooks = {
                 .instance = nc,
                 .read = instance->codec.read,
                 .write = instance->codec.write,
                 .close = instance->codec.close,
        };
        int rc = vector_push(&codec_hooks, &hooks);
        if (rc < 0) {
            hooks.close(nc);
            errno = -rc;
            return NULL;
        }
        instance->codec.read = custom_pdu_read;
        instance->codec.write = custom_pdu_write;
        instance->codec.close = custom_close;
    }

    /* Return the NCodec object. */
    return nc;
}
