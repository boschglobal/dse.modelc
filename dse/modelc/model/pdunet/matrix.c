// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <dse/modelc/model/pdunet/network.h>
#include <dse/modelc/controller/model_private.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif


typedef void (*LinearRangeFunc)(PduNetworkDesc*, PduRange*);
static void _apply_range(PduNetworkDesc* net, PduRange* range, PduDirection dir,
    LinearRangeFunc func)
{
    assert(net);
    if (range == NULL) {
        for (size_t range_idx = 0; range_idx < vector_len(&net->matrix.range);
            range_idx++) {
            PduRange* r = vector_at(&net->matrix.range, range_idx, NULL);
            if (r->dir != dir) continue;
            func(net, r);
        }
    } else {
        if (range->dir != dir) return;
        func(net, range);
    }
}

void pdunet_encode_linear(PduNetworkDesc* net, PduRange* range)
{
    _apply_range(net, range, PduDirectionTx, pdunet_pdu_calculate_linear_range);
}

void pdunet_decode_linear(PduNetworkDesc* net, PduRange* range)
{
    _apply_range(net, range, PduDirectionRx, pdunet_pdu_calculate_linear_range);
}

void pdunet_encode_pack(PduNetworkDesc* net, PduRange* range)
{
    _apply_range(net, range, PduDirectionTx, pdunet_pdu_pack_range);
}

void pdunet_decode_unpack(PduNetworkDesc* net, PduRange* range)
{
    _apply_range(net, range, PduDirectionRx, pdunet_pdu_pack_range);
}


static inline Vector* matrix_vec_at(PduTransformMatrix* m, size_t offset)
{
    return (Vector*)((uint8_t*)m + offset);
}

static int vector_sort_txrx(const void* left, const void* right)
{
    const PduObject* l = left;
    const PduObject* r = right;
    if (l->pdu->dir < r->pdu->dir) return -1;
    if (l->pdu->dir > r->pdu->dir) return 1;
    return 0;
}

typedef struct matrix_item_spec {
    size_t       offset;
    size_t       size;
    VectorCompar func;
} matrix_item_spec;
static const matrix_item_spec matrix_vector_offset_list[] = {
    /* pdu items (pdu_count) */
    { offsetof(PduTransformMatrix, pdu), sizeof(PduObject), vector_sort_txrx },
    { offsetof(PduTransformMatrix, payload), sizeof(uint8_t*), NULL },
    { offsetof(PduTransformMatrix, range), sizeof(PduRange), NULL },
    /* signal items (signal_count) */
    { offsetof(PduTransformMatrix, signal.pdu_idx), sizeof(size_t), NULL },
    { offsetof(PduTransformMatrix, signal.signal_idx), sizeof(size_t), NULL },
    { offsetof(PduTransformMatrix, signal.name), sizeof(const char*), NULL },
    { offsetof(PduTransformMatrix, signal.skip), sizeof(bool), NULL },
    { offsetof(PduTransformMatrix, signal.phys), sizeof(double), NULL },
    { offsetof(PduTransformMatrix, signal.raw), sizeof(uint64_t), NULL },
    { offsetof(PduTransformMatrix, signal.factor), sizeof(double), NULL },
    { offsetof(PduTransformMatrix, signal.offset), sizeof(double), NULL },
    { offsetof(PduTransformMatrix, signal.min), sizeof(double), NULL },
    { offsetof(PduTransformMatrix, signal.max), sizeof(double), NULL },
    { offsetof(PduTransformMatrix, signal.encode), sizeof(int32_t), NULL },
    { offsetof(PduTransformMatrix, signal.decode), sizeof(int32_t), NULL },
    { offsetof(PduTransformMatrix, signal.start_bit), sizeof(uint16_t), NULL },
    { offsetof(PduTransformMatrix, signal.length_bits), sizeof(uint16_t),
        NULL },
};
#define MATRIX_SIGNAL_OFFSET 3
#define MATRIX_RANGE_OFFSET  2

static void _allocate_matrix(
    PduNetworkDesc* net, size_t pdu_count, size_t signal_count)
{
    assert(net);
    // Item 0 .. MATRIX_SIGNAL_OFFSET - 1 (pdu)
    for (size_t i = 0; i < MATRIX_SIGNAL_OFFSET; i++) {
        const matrix_item_spec* m = &matrix_vector_offset_list[i];
        Vector*                 v = matrix_vec_at(&net->matrix, m->offset);
        if (i == MATRIX_RANGE_OFFSET) {
            *v = vector_make(m->size, __PduDirectionCount, m->func);
        } else {
            *v = vector_make(m->size, pdu_count, m->func);
        }
    }
    // Item MATRIX_SIGNAL_OFFSET .. (signal)
    net->matrix.signal.count = signal_count;
    for (size_t i = MATRIX_SIGNAL_OFFSET;
        i < ARRAY_SIZE(matrix_vector_offset_list); i++) {
        const matrix_item_spec* m = &matrix_vector_offset_list[i];
        Vector*                 v = matrix_vec_at(&net->matrix, m->offset);
        *v = vector_make(m->size, signal_count, m->func);
    }
}

void pdunet_matrix_clear(PduNetworkDesc* net)
{
    assert(net);
    for (size_t i = 0; i < vector_len(&net->matrix.payload); i++) {
        uint8_t** payload = vector_at(&net->matrix.payload, i, NULL);
        if (*payload != NULL) free(*payload);
    }
    for (size_t i = 0; i < vector_len(&net->matrix.range); i++) {
        PduRange* range = vector_at(&net->matrix.range, i, NULL);
        vector_reset(&range->pdu_list);
    }
    for (size_t i = 0; i < vector_len(&net->matrix.pdu); i++) {
        PduObject* pdu = vector_at(&net->matrix.pdu, i, NULL);
        if (pdu->ncodec.metadata.lpdu) free(pdu->ncodec.metadata.lpdu);
    }
    for (size_t i = 0; i < ARRAY_SIZE(matrix_vector_offset_list); i++) {
        const matrix_item_spec* m = &matrix_vector_offset_list[i];
        Vector*                 v = matrix_vec_at(&net->matrix, m->offset);
        vector_reset(v);
    }
}


static void _initialise_pdu(PduNetworkDesc* net, PduObject* o)
{
    uint8_t** payload =
        vector_at(&(net->matrix.payload), o->matrix.pdu_idx, NULL);
    assert(payload);
    if (*payload != NULL) {
        free(*payload);
        *payload = NULL;
    }
    if (o->pdu->length) {
        *payload = calloc(o->pdu->length, sizeof(uint8_t));
    }

    // NCodec PDU fields, preset.
    o->ncodec.pdu.id = o->pdu->id;
    o->ncodec.pdu.payload = *payload;
    o->ncodec.pdu.payload_len = o->pdu->length;
    switch (net->network.transport_type) {
    case NCodecPduTransportTypeFlexray:
        break;
    default:
        break;
    };
}

int pdunet_matrix_transform(PduNetworkDesc* net, PduNetworkSortFunc sort)
{
    UNUSED(sort);
    assert(net);

    // Allocate the matrix.
    size_t pdu_count = vector_len(&net->pdus);
    size_t signal_count = 0;
    for (size_t i = 0; i < pdu_count; i++) {
        PduItem* p = vector_at(&net->pdus, i, NULL);
        signal_count += vector_len(&p->signals);
    }
    _allocate_matrix(net, pdu_count, signal_count);

    // Transform net --> matrix.
    size_t signal_offset = 0;
    for (size_t pdu_idx = 0; pdu_idx < pdu_count; pdu_idx++) {
        PduItem* p = vector_at(&net->pdus, pdu_idx, NULL);
        // PDU -> matrix.pdu.
        PduObject o = {
            .pdu = p,
            .matrix = {
                .range = {
                    .offset = signal_offset,
                    .count = vector_len(&p->signals),
                },
            },
            .lua = {
                .encode_ref = p->lua.encode_ref,
                .decode_ref = p->lua.decode_ref,
            }
        };
        vector_push(&(net->matrix.pdu), &o);
        vector_push(&(net->matrix.payload), &(uint8_t*){ NULL });
    }
    vector_sort(&net->matrix.pdu);
    for (size_t pdu_idx = 0; pdu_idx < vector_len(&net->matrix.pdu);
        pdu_idx++) {
        PduObject* o = vector_at(&(net->matrix.pdu), pdu_idx, NULL);

        // Correct the matrix.pdu_idx (after sorting).
        o->matrix.pdu_idx = pdu_idx;
        o->matrix.range.offset = signal_offset;
        log_trace("Matrix: Pdu[%u]%s: offset=%u, length=%u", pdu_idx,
            o->pdu->name, signal_offset, vector_len(&o->pdu->signals));

        // Set the schedule.
        PduItem* p = o->pdu;
        assert(p);
        if (p->schedule.interval > 0.0) {
            uint32_t interval = p->schedule.interval / net->schedule.step_size;
            o->schedule.interval = interval;
            log_trace("Matrix: Pdu[%u]%s: interval=%u", pdu_idx, o->pdu->name,
                o->schedule.interval);
        } else {
            o->schedule.interval = 0;
        }
        if (p->schedule.phase != 0.0 && o->schedule.interval) {
            // Phase only applys for PDUs with a schedule interval.
            int32_t phase = p->schedule.phase / net->schedule.step_size;
            // Constraint phase to the bounds of the interval.
            if (phase > 0) {
                phase = phase % o->schedule.interval;
            } else {
                phase = -(abs(phase) % o->schedule.interval);
            }
            // Internally phase is absolute to the epoch, rather than relative
            // to the interval.
            o->schedule.phase = o->schedule.interval + phase;
            log_trace("Matrix: Pdu[%u]%s: phase=%u", pdu_idx, o->pdu->name,
                o->schedule.phase);
        } else {
            o->schedule.phase = 0;
        }

        // Signal -> matrix.signal.
        for (size_t sig_idx = 0; sig_idx < vector_len(&o->pdu->signals);
            sig_idx++) {
            PduSignalItem* s = vector_at(&o->pdu->signals, sig_idx, NULL);
            log_trace("Matrix:[%u]  Signal[%u]%s: encode_ref=%d, decode_ref=%d",
                signal_offset + sig_idx, sig_idx, s->name, s->lua.encode_ref,
                s->lua.decode_ref);

            // Index to matrix pdu, not net pdu.
            vector_push(&net->matrix.signal.pdu_idx, &pdu_idx);
            vector_push(&net->matrix.signal.signal_idx, &sig_idx);

            vector_push(&net->matrix.signal.name, &s->name);
            vector_push(&net->matrix.signal.skip, &(bool){ false });
            vector_push(&net->matrix.signal.phys, &(double){ 0.0 });
            vector_push(&net->matrix.signal.raw, &(uint64_t){ 0 });
            vector_push(&net->matrix.signal.factor, &s->factor);
            vector_push(&net->matrix.signal.offset, &s->offset);
            vector_push(&net->matrix.signal.min, &s->min);
            vector_push(&net->matrix.signal.max, &s->max);
            vector_push(&net->matrix.signal.encode, &s->lua.encode_ref);
            vector_push(&net->matrix.signal.decode, &s->lua.decode_ref);
            vector_push(&net->matrix.signal.start_bit, &s->start_bit);
            vector_push(&net->matrix.signal.length_bits, &s->length_bits);
        }
        signal_offset += vector_len(&o->pdu->signals);
    }

    // Allocate NCodec PDU objects.
    for (size_t i = 0; i < vector_len(&net->matrix.pdu); i++) {
        PduObject* o = vector_at(&(net->matrix.pdu), i, NULL);
        _initialise_pdu(net, o);
    }

    // Range objects.
    size_t       sig_count = 0;
    size_t       sig_offset = 0;
    PduDirection d = PduDirectionNone;
    Vector       pdu_list = vector_make(sizeof(size_t), 4, NULL);
    for (size_t i = 0; i < vector_len(&net->matrix.pdu); i++) {
        PduObject* o = vector_at(&(net->matrix.pdu), i, NULL);
        if (o->pdu->dir == d) {
            sig_count += o->matrix.range.count;
            vector_push(&pdu_list, &(size_t){ o->matrix.pdu_idx });
            continue;  // Next PDU.
        } else {
            // Emit the current PDU Range.
            if (sig_count > 0) {
                if (d != PduDirectionNone) {
                    PduRange r = { .dir = d,
                        .offset = sig_offset,
                        .length = sig_count,
                        .pdu_list = pdu_list };
                    vector_push(&net->matrix.range, &r);
                    pdu_list = (Vector){ 0 };
                }
                sig_offset += sig_count;
            }
            // Setup for the next PDU Range (this PDU).
            sig_count = o->matrix.range.count;
            d = o->pdu->dir;
            vector_reset(&pdu_list);
            pdu_list = vector_make(sizeof(size_t), 4, NULL);
            vector_push(&pdu_list, &(size_t){ o->matrix.pdu_idx });
            continue;  // Next PDU and next PduDirection
        }
    }
    if (sig_count > 0 && d != PduDirectionNone) {
        PduRange r = { .dir = d,
            .offset = sig_offset,
            .length = sig_count,
            .pdu_list = pdu_list };
        vector_push(&net->matrix.range, &r);
        pdu_list = (Vector){ 0 };
    }
    vector_reset(&pdu_list);

    return 0;
}


void pdunet_pdu_calculate_linear_range(PduNetworkDesc* net, PduRange* r)
{
    assert(net);
    assert(net->mi);
    assert(net->mi->private);
    ModelInstancePrivate* mip = net->mi->private;
    lua_State*            L = mip->lua_state;
    errno = 0;

    // clang-format off
    bool* skip = (bool*)vector_at(&net->matrix.signal.skip, r->offset, NULL);
    double* phys = (double*)vector_at(&net->matrix.signal.phys, r->offset, NULL);
    double* factor = (double*)vector_at(&net->matrix.signal.factor, r->offset, NULL);
    double* offset = (double*)vector_at(&net->matrix.signal.offset, r->offset, NULL);
    double* min = (double*)vector_at(&net->matrix.signal.min, r->offset, NULL);
    double* max = (double*)vector_at(&net->matrix.signal.max, r->offset, NULL);
    uint64_t* raw = (uint64_t*)vector_at(&net->matrix.signal.raw, r->offset, NULL);
    int32_t* encode = (int32_t*)vector_at(&net->matrix.signal.encode, r->offset, NULL);
    int32_t* decode = (int32_t*)vector_at(&net->matrix.signal.decode, r->offset, NULL);
    size_t* pdu_idx = (size_t*)vector_at(&net->matrix.signal.pdu_idx, r->offset, NULL);
    // clang-format on

    /* Linear function. */
    switch (r->dir) {
    case PduDirectionTx:
        for (size_t i = 0; i < r->length; i++) {
            /* Schedule visitor will set skip is PDU/Signal should not update.*/
            if (LIKELY(skip[i])) continue;
            /* Constraints on the calculation. */
            // TODO: can these constrains be removed from the matrix?
            if (!isnan(max[i]) && phys[i] > max[i]) continue;
            if (!isnan(min[i]) && phys[i] < min[i]) continue;
            if (isnan(factor[i]) || isnan(offset[i])) continue;
            /* Calculate the raw value. */
            raw[i] = (phys[i] - offset[i]) / factor[i];
            /* Call the Lua function. */
            if (encode[i] > 0) {
                double   _phys = phys[i];
                uint64_t _raw = raw[i];
                uint8_t* payload = NULL;
                vector_at(&(net->matrix.payload), pdu_idx[i], &payload);
                PduObject* o = vector_at(&(net->matrix.pdu), pdu_idx[i], NULL);

                log_trace(
                    "Lua Call: signal encode[%u]: phys=%f, raw=%u, func=%d",
                    r->offset + i, _phys, _raw, encode[i]);
                pdunet_lua_signal_call(
                    L, encode[i], &_phys, &_raw, payload, o->pdu->length);
                log_trace("  phys=%f, raw=%u", _phys, _raw);
                raw[i] = _raw;
                for (bool _ = (_phys != phys[i]); _; _ = false) {
                    phys[i] = _phys;
                    /* Phys value changed, update raw. */
                    if (!isnan(max[i]) && phys[i] > max[i]) break;
                    if (!isnan(min[i]) && phys[i] < min[i]) break;
                    if (isnan(factor[i]) || isnan(offset[i])) break;
                    /* Update the raw value. */
                    raw[i] = (phys[i] - offset[i]) / factor[i];
                    log_trace("  phys=%f, raw=%u (updated)", phys[i], raw[i]);
                }
            }
        }
        break;
    case PduDirectionRx:
        for (size_t i = 0; i < r->length; i++) {
            /* Call the Lua function. */
            if (decode[i] > 0) {
                double   _phys = phys[i];
                uint64_t _raw = raw[i];
                uint8_t* payload = NULL;
                vector_at(&(net->matrix.payload), pdu_idx[i], &payload);
                PduObject* o = vector_at(&(net->matrix.pdu), pdu_idx[i], NULL);

                log_trace(
                    "Lua Call: Signal Decode[%u]: phys=%f, raw=%u, func=%d",
                    r->offset + i, _phys, _raw, decode[i]);
                pdunet_lua_signal_call(
                    L, decode[i], &_phys, &_raw, payload, o->pdu->length);
                log_trace("  phys=%f, raw=%u", _phys, _raw);
                phys[i] = _phys;
                for (bool _ = (_raw != raw[i]); _; _ = false) {
                    raw[i] = _raw;
                    /* Raw value changed, update phys. */
                    if (isnan(factor[i]) || isnan(offset[i])) break;
                    double val = (raw[i] * factor[i]) + offset[i];
                    if (!isnan(max[i]) && val > max[i]) break;
                    if (!isnan(min[i]) && val < min[i]) break;
                    /* Update the phys value. */
                    phys[i] = val;
                    log_trace("  phys=%f, raw=%u (updated)", phys[i], raw[i]);
                }
            }

            /* Calculate the raw value. */
            if (isnan(factor[i]) || isnan(offset[i])) continue;
            double val = (raw[i] * factor[i]) + offset[i];
            if (!isnan(max[i]) && val > max[i]) continue;
            if (!isnan(min[i]) && val < min[i]) continue;
            phys[i] = val;
        }
        break;
    default:
        break;
    }
}


void pdunet_pdu_pack_range(PduNetworkDesc* net, PduRange* r)
{
    assert(net);
    assert(net->mi);
    assert(net->mi->private);
    ModelInstancePrivate* mip = net->mi->private;
    lua_State*            L = mip->lua_state;

    size_t* pdu_idx =
        (size_t*)vector_at(&net->matrix.signal.pdu_idx, r->offset, NULL);
    size_t* signal_idx =
        (size_t*)vector_at(&net->matrix.signal.signal_idx, r->offset, NULL);
    uint64_t* raw =
        (uint64_t*)vector_at(&net->matrix.signal.raw, r->offset, NULL);
    uint16_t* start =
        (uint16_t*)vector_at(&net->matrix.signal.start_bit, r->offset, NULL);
    uint16_t* length =
        (uint16_t*)vector_at(&net->matrix.signal.length_bits, r->offset, NULL);

    switch (r->dir) {
    case PduDirectionTx:
        /* Pack from raw to payload. */
        for (size_t i = 0; i < r->length; i++) {
            size_t   bit_count = 0;
            size_t   byte_idx = start[i] / 8;
            uint64_t value = raw[i];
            uint8_t* payload = NULL;
            vector_at(&(net->matrix.payload), pdu_idx[i], &payload);
            PduObject*     o = vector_at(&(net->matrix.pdu), pdu_idx[i], NULL);
            PduSignalItem* s = vector_at(&o->pdu->signals, signal_idx[i], NULL);
            log_trace("Write Payload[%u][%s]: start=%u, length=%u, value=%08x",
                pdu_idx[i], s->name, start[i], length[i], value);

            if (start[i] % 8 != 0) {
                // Write the first N bits at O bit offset.
                size_t  O = start[i] % 8;
                size_t  N = ((8 - O) > length[i]) ? length[i] : 8 - O;
                uint8_t M_p = ((1u << N) - 1u) << (O);
                uint8_t M_v = ((1u << N) - 1u);
                uint8_t V = ((uint8_t)value & M_v) << (O);
                payload[byte_idx] = (payload[byte_idx] & ~M_p) | V;
                log_trace("  Write[%u]=%02x O=%u, N=%u, M_p=%02x, M_v=%02x, "
                          "value=%08x",
                    byte_idx, V, O, N, M_p, M_v, value);
                byte_idx++;
                bit_count += N;
                value = value >> N;
            }
            while (bit_count + 8 < length[i]) {
                size_t  O = 0;
                size_t  N = 8;
                uint8_t M_p = 0xff;
                uint8_t M_v = 0xff;
                uint8_t V = (uint8_t)value;
                payload[byte_idx] = V;
                log_trace("  Write[%u]=%02x O=%u, N=%u, M_p=%02x, M_v=%02x, "
                          "value=%08x",
                    byte_idx, V, O, N, M_p, M_v, value);
                byte_idx++;
                bit_count += 8;
                value = value >> 8;
            }
            if (bit_count < length[i]) {
                // Write the last N bits.
                size_t  O = 0;
                size_t  N = length[i] - bit_count;
                uint8_t M_p = ((1u << N) - 1u) << (O);
                uint8_t M_v = ((1u << N) - 1u);
                uint8_t V = ((uint8_t)value & M_v) << (O);
                payload[byte_idx] = (payload[byte_idx] & ~M_p) | V;
                log_trace("  Write[%u]=%02x O=%u, N=%u, M_p=%02x, M_v=%02x, "
                          "value=%08x",
                    byte_idx, V, O, N, M_p, M_v, value);
            }
        }
        /* Call Lua functions, modify payload. */
        for (size_t i = 0; i < vector_len(&r->pdu_list); i++) {
            size_t pdu_idx = 0;
            vector_at(&r->pdu_list, i, &pdu_idx);
            uint8_t* payload = NULL;
            vector_at(&(net->matrix.payload), pdu_idx, &payload);
            PduObject* o = vector_at(&(net->matrix.pdu), pdu_idx, NULL);
            if (o->lua.encode_ref > 0) {
                log_trace("Lua Call: PDU Pack Tx[%u]: func=%d", pdu_idx,
                    o->lua.encode_ref);
                pdunet_lua_pdu_call(
                    L, o->lua.encode_ref, payload, o->pdu->length);
            }
        }
        break;
    case PduDirectionRx:
        /* Call Lua functions, modify payload. */
        for (size_t i = 0; i < vector_len(&r->pdu_list); i++) {
            size_t pdu_idx = 0;
            vector_at(&r->pdu_list, i, &pdu_idx);
            uint8_t* payload = NULL;
            vector_at(&(net->matrix.payload), pdu_idx, &payload);
            PduObject* o = vector_at(&(net->matrix.pdu), pdu_idx, NULL);
            if (o->lua.decode_ref > 0) {
                log_trace("Lua Call: PDU Unpack Rx[%u]: func=%d", pdu_idx,
                    o->lua.encode_ref);
                pdunet_lua_pdu_call(
                    L, o->lua.decode_ref, payload, o->pdu->length);
            }
        }
        /* Unpack from payload to raw. */
        for (size_t i = 0; i < r->length; i++) {
            size_t   bit_count = 0;
            size_t   byte_idx = start[i] / 8;
            uint64_t value = 0;
            uint8_t* payload = NULL;
            vector_at(&(net->matrix.payload), pdu_idx[i], &payload);
            PduObject*     o = vector_at(&(net->matrix.pdu), pdu_idx[i], NULL);
            PduSignalItem* s = vector_at(&o->pdu->signals, signal_idx[i], NULL);
            log_trace("Read Payload[%u][%s]: start=%u, length=%u", pdu_idx[i],
                s->name, start[i], length[i]);

            if (start[i] % 8 != 0) {
                // Read the first N bits at O bit offset.
                size_t  O = start[i] % 8;
                size_t  N = ((8 - O) > length[i]) ? length[i] : 8 - O;
                uint8_t M_v = ((1u << N) - 1u);
                uint8_t V = (payload[byte_idx] >> O) & M_v;
                value = V;
                log_trace("  Read[%u]=%02x O=%u, N=%u, M_v=%02x, payload=%02x, "
                          "value=%08x",
                    byte_idx, V, O, N, M_v, payload[byte_idx], value);
                byte_idx++;
                bit_count += N;
            }
            while (bit_count + 8 < length[i]) {
                size_t  O = 0;
                size_t  N = 8;
                uint8_t M_v = 0xff;
                uint8_t V = payload[byte_idx];
                value |= ((uint64_t)V << bit_count);
                log_trace("  Read[%u]=%02x O=%u, N=%u, M_v=%02x, payload=%02x, "
                          "value=%08x",
                    byte_idx, V, O, N, M_v, payload[byte_idx], value);
                byte_idx++;
                bit_count += 8;
            }
            if (bit_count < length[i]) {
                // Read the last N bits.
                size_t  O = 0;
                size_t  N = length[i] - bit_count;
                uint8_t M_v = ((1u << N) - 1u);
                uint8_t V = (payload[byte_idx] >> O) & M_v;
                value |= ((uint64_t)V << bit_count);
                log_trace("  Read[%u]=%02x O=%u, N=%u, M_v=%02x, payload=%02x, "
                          "value=%08x",
                    byte_idx, V, O, N, M_v, payload[byte_idx], value);
            }
            raw[i] = value;
        }
        break;
    default:
        break;
    }
}
