// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_PDUNET_H_
#define DSE_MODELC_PDUNET_H_

#include <stdint.h>
#include <stdbool.h>
#include <dse/clib/collections/vector.h>
#include <dse/clib/data/marshal.h>
#include <dse/modelc/model.h>
#include <dse/modelc/schema.h>


/* DLL Interface visibility. */
#if defined _WIN32 || defined __CYGWIN__
#ifdef DLL_BUILD
#define DLL_PUBLIC __declspec(dllexport)
#else
#define DLL_PUBLIC __declspec(dllimport)
#endif /* DLL_BUILD */
#define DLL_PRIVATE
#else /* Linux */
#define DLL_PUBLIC  __attribute__((visibility("default")))
#define DLL_PRIVATE __attribute__((visibility("hidden")))
#endif /* _WIN32 || defined __CYGWIN__ */


/**
PDU API
=======

(Part of Model API)

*/

typedef struct PduNetworkDesc PduNetworkDesc;


typedef struct PduSignalItem {
    const char* name;
    /* Signal properties. */
    uint16_t    start_bit;
    uint16_t    length_bits;
    double      factor; /* Prohibited value 0. */
    double      offset;
    double      min; /* Optional, set NaN. */
    double      max; /* Optional, set Nan. */
    /* Functions. */
    struct {
        const char* encode;
        const char* decode;
        int         encode_ref;
        int         decode_ref;
    } lua;
} PduSignalItem;


typedef enum {
    PduDirectionNone = 0,
    PduDirectionRx = 1, /* "Rx" */
    PduDirectionTx = 2, /* "Tx" */
    __PduDirectionCount = 3,
} PduDirection;


typedef struct PduItem {
    const char*  name;
    /* PDU properties. */
    uint32_t     id;
    size_t       length;
    PduDirection dir;  // TODO: schema update
    /* Schedule. */
    struct {
        double phase;     // TODO: schema update
        double interval;  // TODO: schema update
    } schedule;
    /* Functions. */
    struct {
        const char* encode;
        const char* decode;
        int         encode_ref;
        int         decode_ref;
    } lua;
    /* Metadata. */
    struct {
        void* config;
    } metadata;
    /* Signals. */
    Vector signals; /* PduSignalItem. */
} PduItem;


typedef struct PduObject {  // FIXME: internal type ??
    PduItem* pdu;
    bool     needs_tx;
    bool     update_signals;
    uint32_t checksum; /* Detects changed payload. */
    struct {
        size_t pdu_idx;
        struct {
            size_t offset; /* Offset into matrix.signal */
            size_t count;
        } range;
    } matrix;
    struct {
        uint32_t interval; /* Normalised value, factor of step_size. */
        uint32_t phase;    /* Normalised value, factor of step_size. */
    } schedule;
    struct {
        int encode_ref;
        int decode_ref;
    } lua;
    struct {
        /* NCodec Objects. */
        // NCodecPdu pdu;
        struct {
            uint32_t       id;
            const uint8_t* payload;
            size_t         payload_len;
        } pdu;
        struct {
            void* lpdu;
        } metadata;
    } ncodec;
} PduObject;


typedef struct PduRange {
    size_t       offset;
    size_t       length;
    Vector       pdu_list; /* o.matrix.pdu_idx / size_t */
    /* Default range criteria is direction.*/
    PduDirection dir;
    /* Complex sort may produce specific range criteria. */
    void*        sort_object;
} PduRange;


/* Operate on this structure, vector optimised. */
typedef struct PduTransformMatrix {  // FIXME: internal type ??
    /* PDU Objects, sorted on pdu.tx/rx then pdu.id. */
    Vector pdu;     /* PduObject */
    Vector payload; /* uint8_t*, allocated vector of payloads. */
    /* Range objects, resultant from sorting. */
    Vector range;

    /* Signal matrix, sorted by func, default is pdu.tx/rx then pdu.id. */
    struct {
        size_t count;
        /* Indexes. */
        Vector pdu_idx;    /* size_t, matrix.pdu */
        Vector signal_idx; /* size_t, matrix.signal */
        /* Signal. */
        Vector name; /* const char* */
        /* Schedule. */
        Vector skip; /* bool, 0=update (default, 1 = skip matrix row) */
        /* Linear Transform. */
        Vector phys;   /* double, signal value (physical) */
        Vector raw;    /* uint64_t, signal value (raw) */
        Vector factor; /* double, prohibited value 0 */
        Vector offset; /* double */
        Vector min;    /* double, clamps value */
        Vector max;    /* double, clamps value */
        /* Complex (Lua) Transform. */
        Vector encode; /* Lua function handle (int32_t) */
        Vector decode; /* Lua function handle (int32_t) */
        /* Encoding. */
        Vector start_bit;   /* uint16_t */
        Vector length_bits; /* uint16_t */
    } signal;
} PduTransformMatrix;


typedef void (*PduNetworkParseNetwork)(PduNetworkDesc* net);
typedef void (*PduNetworkParsePdu)(PduItem* pdu, void* n);
typedef void (*PduNetworkNCodecPduTx)(PduNetworkDesc* net);

typedef struct PduNetworkNCodecVTable {
    PduNetworkParseNetwork parse_network;
    PduNetworkParsePdu     parse_pdu;

    PduNetworkNCodecPduTx config;
    PduNetworkNCodecPduTx lpdu_tx;
    PduNetworkNCodecPduTx lpdu_rx;
    PduNetworkNCodecPduTx status;

    /* Stateful Info. */
    bool config_done;
    struct {
        uint8_t  cycle;
        uint32_t cycle_time; /* Normalised (to step_size). */
    } flexray;
} PduNetworkNCodecVTable;


typedef struct PduNetworkDesc {
    const char*        name;
    void*              ncodec;
    ModelInstanceSpec* mi;
    void*              doc; /* YamlNode */

    /* Network properties. */
    struct {
        const char* mime_type;
        const char* signal;

        uint32_t               transport_type; /* NCodecPduTransportType */
        PduNetworkNCodecVTable vtable;
        struct {
            void* config;
        } metadata;
    } network;

    /* Schedule. */
    struct {
        uint32_t simulation_time; /* Normalised (to step_size). */
        double   step_size;
        double   epoch_offset;  // TODO: schema update
    } schedule;

    /* Functions. */
    struct {
        const char* global;
    } lua;

    /* PDUs (and Signals) parsed from Network YAML. */
    Vector pdus; /* PduItem */

    /* Transformed matrix (of PduObject objects and matrix vectors). */
    PduTransformMatrix matrix;

    /* Marshal Signal Map (to SignalVector, NTL). */
    MarshalSignalMap* msm;
} PduNetworkDesc;


typedef void (*PduNetworkSortFunc)(
    PduNetworkDesc* net, PduRange* range, void* data);
typedef void (*PduNetworkVisitFunc)(
    PduNetworkDesc* net, PduObject* pdu, void* data);

/* Public API. */
DLL_PUBLIC PduNetworkDesc* pdunet_create(ModelInstanceSpec* mi, void* ncodec,
    SchemaLabel* net_labels, SchemaLabel* sg_labels, PduNetworkSortFunc sort,
    void* data);

DLL_PUBLIC PduNetworkDesc* pdunet_find(ModelInstanceSpec* mi, void* ncodec);

DLL_PUBLIC void pdunet_visit(PduNetworkDesc* net, PduRange* range,
    PduNetworkVisitFunc visit, void* data);
DLL_PUBLIC void pdunet_tx(PduNetworkDesc* net, PduRange* range,
    PduNetworkVisitFunc visit, void* data, double simulation_time);
DLL_PUBLIC void pdunet_rx(PduNetworkDesc* net, PduRange* range,
    PduNetworkVisitFunc visit, void* data);

DLL_PUBLIC void pdunet_destroy(PduNetworkDesc* net);

DLL_PUBLIC void pdunet_visit_clear_update_flag(
    PduNetworkDesc* net, PduObject* pdu, void* data);
DLL_PUBLIC void pdunet_visit_clear_tx_flag(
    PduNetworkDesc* net, PduObject* pdu, void* data);
DLL_PUBLIC void pdunet_visit_clear_checksum(
    PduNetworkDesc* net, PduObject* pdu, void* data);
DLL_PUBLIC void pdunet_visit_needs_tx(
    PduNetworkDesc* net, PduObject* pdu, void* data);

#endif  // DSE_MODELC_PDUNET_H_
