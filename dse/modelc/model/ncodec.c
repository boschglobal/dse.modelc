// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdint.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/ncodec/codec.h>
#include <dse/modelc/model.h>


#define UNUSED(x) ((void)x)


/* Stream Interface for binary signals (supports NCodec). */
typedef struct __BinarySignalStream {
    NCodecStreamVTable s;
    /**/
    SignalVector*      sv;
    uint32_t           idx;
    uint32_t           pos;
} __BinarySignalStream;


static size_t stream_read(NCODEC* nc, uint8_t** data, size_t* len, int pos_op)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc == NULL || _nc->stream == NULL) return -ENOSTR;
    if (data == NULL || len == NULL) return -EINVAL;

    __BinarySignalStream* _s = (__BinarySignalStream*)_nc->stream;
    uint32_t              s_len = _s->sv->length[_s->idx];
    uint8_t*              s_buffer = _s->sv->binary[_s->idx];

    /* Check if any buffer. */
    if (s_buffer == NULL) {
        *data = NULL;
        *len = 0;
        return 0;
    }
    /* Check EOF. */
    if (_s->pos >= s_len) {
        *data = NULL;
        *len = 0;
        return 0;
    }
    /* Return buffer, from current pos. */
    *data = &s_buffer[_s->pos];
    *len = s_len - _s->pos;
    /* Advance the position indicator. */
    if (pos_op == NCODEC_POS_UPDATE) _s->pos = s_len;

    return *len;
}

static size_t stream_write(NCODEC* nc, uint8_t* data, size_t len)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc == NULL || _nc->stream == NULL) return -ENOSTR;

    __BinarySignalStream* _s = (__BinarySignalStream*)_nc->stream;
    uint32_t              s_len = _s->sv->length[_s->idx];

    /* Write from current pos (i.e. truncate). */
    if (_s->pos > s_len) _s->pos = s_len;
    _s->sv->length[_s->idx] = _s->pos;
    _s->sv->vtable.append(_s->sv, _s->idx, data, len);
    _s->pos += len;

    return len;
}

static int64_t stream_seek(NCODEC* nc, size_t pos, int op)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc && _nc->stream) {
        __BinarySignalStream* _s = (__BinarySignalStream*)_nc->stream;
        uint32_t              s_len = _s->sv->length[_s->idx];

        if (op == NCODEC_SEEK_SET) {
            if (pos > s_len) {
                _s->pos = s_len;
            } else {
                _s->pos = pos;
            }
        } else if (op == NCODEC_SEEK_CUR) {
            pos = _s->pos + pos;
            if (pos > s_len) {
                _s->pos = s_len;
            } else {
                _s->pos = pos;
            }
        } else if (op == NCODEC_SEEK_END) {
            _s->pos = s_len;
        } else if (op == NCODEC_SEEK_RESET) {
            /* Reset before stream_write has truncate effect. */
            _s->pos = 0;
            _s->sv->length[_s->idx] = 0;
            _s->sv->reset_called[_s->idx] = true;
        } else {
            return -EINVAL;
        }
        return _s->pos;
    }
    return -ENOSTR;
}

static int64_t stream_tell(NCODEC* nc)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc && _nc->stream) {
        __BinarySignalStream* _s = (__BinarySignalStream*)_nc->stream;
        return _s->pos;
    }
    return -ENOSTR;
}

static int stream_eof(NCODEC* nc)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc && _nc->stream) {
        __BinarySignalStream* _s = (__BinarySignalStream*)_nc->stream;
        uint32_t              s_len = _s->sv->length[_s->idx];

        if (_s->pos < s_len) return 0;
    }
    return 1;
}

static int stream_close(NCODEC* nc)
{
    UNUSED(nc);

    return 0;
}


/* Private stream interface. */

void* model_sv_stream_create(SignalVector* sv, uint32_t idx)
{
    if (sv->is_binary != true) {
        errno = EINVAL;
        return NULL;
    }
    __BinarySignalStream* stream = calloc(1, sizeof(__BinarySignalStream));
    stream->s = (struct NCodecStreamVTable){
        .read = stream_read,
        .write = stream_write,
        .seek = stream_seek,
        .tell = stream_tell,
        .eof = stream_eof,
        .close = stream_close,
    };
    stream->sv = sv;
    stream->idx = idx;
    stream->pos = 0;

    return stream;
}

void model_sv_stream_destroy(void* stream)
{
    if (stream) free(stream);
}


/* NCodec Interface. */

NCODEC* ncodec_open(const char* mime_type, NCodecStreamVTable* stream)
{
    NCODEC* nc = ncodec_create(mime_type);
    if (nc == NULL || stream == NULL) {
        errno = EINVAL;
        return NULL;
    }
    NCodecInstance* _nc = (NCodecInstance*)nc;
    _nc->stream = stream;
    return nc;
}
