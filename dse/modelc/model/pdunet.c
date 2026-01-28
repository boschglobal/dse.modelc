// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <dse/logger.h>
#include <dse/clib/collections/vector.h>
#include <dse/modelc/model/pdunet/network.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/pdunet.h>
#include <dse/ncodec/codec.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/**
pdunet_create
=============

Create and configure an `PduNetworkDesc` object to represent an PDU Network.

Parameters
----------
mi (ModelInstanceSpec*)
: ModelInstance object.

NCODEC (void*)
: NCodec object.

sort (PduNetworkSortFunc)
: Sort callback function for producing ordered PduRange objects. Optional.

data (void*)
: Data object (pointer) passed to sort callback function. Optional.

Returns
-------
PduNetworkDesc (*)
: PduNetworkDesc object.
*/
PduNetworkDesc* pdunet_create(ModelInstanceSpec* mi, void* ncodec,
    SchemaLabel* net_labels, SchemaLabel* sg_labels, PduNetworkSortFunc sort,
    void* data)
{
    UNUSED(sort);
    UNUSED(data);

    PduNetworkDesc* net = calloc(1, sizeof(PduNetworkDesc));
    if (net == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    *net = (PduNetworkDesc){
        .ncodec = ncodec,
        .mi = mi,
        .pdus = vector_make(sizeof(PduItem), 10, NULL),
    };
    if (net->schedule.step_size <= 0) {
        net->schedule.step_size = MODEL_DEFAULT_STEP_SIZE;
    }
    log_notice("PDU Net: Step size: %f", net->schedule.step_size);
    if (ncodec == NULL || net_labels == NULL) return net;

    /* Parse the network. */
    int rc;
    log_notice("PDU Net: Search for Network");
    for (SchemaLabel* l = net_labels; l->name; l++) {
        log_notice("  Label %s=%s", l->name, l->value);
    }
    rc = pdunet_parse(net, net_labels);
    if (rc == 0) {
        rc = pdunet_configure(net);
        if (rc != 0) {
            log_error("Configure fail: rc=%d", rc);
            return net;
        }
        rc = pdunet_transform(net, NULL);
        if (rc != 0) {
            log_error("Transform fail: rc=%d", rc);
            return net;
        }
    } else if (rc == -ENODATA) {
        log_fatal("Parse fail: Network not found in YAML files (rc=%d)", rc);
    } else {
        log_fatal("Parse fail: rc=%d", rc);
        return net;
    }

    /* Setup marshalling to signals. */
    if (sg_labels) {
        SchemaLabel* channel = NULL;
        log_notice("PDU Net: Search for SignalGroup (Network=%s)", net->name);
        for (SchemaLabel* l = sg_labels; l->name; l++) {
            log_notice("  Label %s=%s", l->name, l->value);
            if (strcmp(l->name, "channel") == 0) channel = l;
        }
        if (channel == NULL) {
            log_error("Annotation 'channel' not found!");
            return net;
        }
        pdunet_build_msm(net, channel->value);
        if (net->msm == NULL) {
            log_error("Marshal table not created");
            return net;
        }
        for (SignalVector* sv = net->mi->model_desc->sv; sv && sv->name; sv++) {
            if (strcmp(sv->alias, channel->value) != 0) continue;
            log_notice("  SignalVector <-> Network Mapping for: %s", sv->name);
            for (uint32_t i = 0; i < net->msm->count; i++) {
                log_notice("    Signal: %s (%d) <-> %s (%d)",
                    sv->signal[net->msm->signal.index[i]],
                    net->msm->signal.index[i],
                    *(const char**)vector_at(&net->matrix.signal.name,
                        net->msm->source.index[i], NULL),
                    net->msm->source.index[i]);
            }
            break;
        }
    }

    return net;
}


/**
pdunet_find
===========

Create and configure an `PduNetworkDesc` object to represent an PDU Network.

Parameters
----------
mi (ModelInstanceSpec*)
: ModelInstance object.

NCODEC (void*)
: NCodec object.

Returns
-------
PduNetworkDesc (struct)
: PduNetworkDesc object.
*/
PduNetworkDesc* pdunet_find(ModelInstanceSpec* mi, void* ncodec)
{
    if (mi == NULL) return NULL;
    if (ncodec == NULL) return NULL;

    ModelInstancePrivate* mip = mi->private;
    if (mip) {
        for (size_t i = 0; i < vector_len(&mip->pdunet); i++) {
            PduNetworkDesc* net = NULL;
            vector_at(&mip->pdunet, i, &net);
            if (net) return net;
        }
    }
    return NULL;
}


/**
pdunet_visit
============

Call a visitor function for each PDU in the PDU Network.

Parameters
----------
net (PduNetworkDesc*)
: PDU Network object.

range (PduRange*)
: Range object, optional. When NULL the visitor function is called for all PDUs
  in the PDU Network.

visit (PduNetworkVisitFunc)
: Visit callback function, called for each PDU Object in the provided range.

data (void*)
: Data object (pointer) passed to visit callback function. Optional.
*/
void pdunet_visit(
    PduNetworkDesc* net, PduRange* range, PduNetworkVisitFunc visit, void* data)
{
    UNUSED(range);
    if (net == NULL || visit == NULL) return;

    for (size_t i = 0; i < vector_len(&net->matrix.pdu); i++) {
        PduObject* pdu = vector_at(&net->matrix.pdu, i, NULL);
        if (pdu) visit(net, pdu, data);
    }
}


/**
pdunet_tx
=========

Transmit PDUs to the configured NCodec object. If a visitor is provided, then
call the visitor before transmitting a PDU, and only transmit the PDU
if the `needs_tx` is set on the `PduObject` after the visitor returns.

Parameters
----------
net (PduNetworkDesc*)
: PDU Network object.

range (PduRange*)
: Range object, optional. When NULL the visitor function is called for all PDUs
  in the PDU Network.

visit (PduNetworkVisitFunc)
: Visit callback function, called for each PDU Object in the provided range.

data (void*)
: Data object (pointer) passed to visit callback function. Optional.
*/
void pdunet_tx(PduNetworkDesc* net, PduRange* range, PduNetworkVisitFunc visit,
    void* data, double simulation_time)
{
    UNUSED(range);
    UNUSED(visit);
    UNUSED(data);

    if (net == NULL) return;
    if (simulation_time < 0) simulation_time = 0.0;

    /* Marshal from SignalVector to PDU Network. */
    marshal_signalmap_out(net->msm);

    log_debug("PDU Net: TX");
    ncodec_truncate(net->ncodec);

    /* Configuration (if network requires). */
    if (net->network.vtable.config_done == false) {
        if (net->network.vtable.config) {
            net->network.vtable.config(net);
        }
        net->network.vtable.config_done = true;
    }

    /* Schedule, based on normalised simulation time. */
    net->schedule.simulation_time = simulation_time / net->schedule.step_size;
    pdunet_schedule(net);

    /* Encode PDUs, call visitor, then Tx. */
    pdunet_encode_linear(net, NULL);
    pdunet_encode_pack(net, NULL);
    if (visit) pdunet_visit(net, range, visit, data);
    if (net->network.vtable.lpdu_tx) {
        net->network.vtable.lpdu_tx(net);
    }

    /* Status (if network requires). */
    if (net->network.vtable.status) {
        net->network.vtable.status(net);
    }

    ncodec_flush(net->ncodec);

    /* Marshal from PDU Network to SignalVector (update changed signals). */
    // TODO: trigger on actual Tx to reduce CPU consumption in idle steps.
    marshal_signalmap_in(net->msm);
}


/**
pdunet_rx
=========

Receive PDUs from the configured NCodec object. If a visitor is provided, then
call the visitor after a PDU is received.

Parameters
----------
net (PduNetworkDesc*)
: PDU Network object.

range (PduRange*)
: Range object, optional. When NULL the visitor function is called for all PDUs
  in the PDU Network.

visit (PduNetworkVisitFunc)
: Visit callback function, called for each PDU Object in the provided range.

data (void*)
: Data object (pointer) passed to visit callback function. Optional.
*/
void pdunet_rx(
    PduNetworkDesc* net, PduRange* range, PduNetworkVisitFunc visit, void* data)
{
    UNUSED(range);
    UNUSED(visit);
    UNUSED(data);

    if (net == NULL) return;

    log_debug("PDU Net: RX");
    ncodec_seek(net->ncodec, 0, NCODEC_SEEK_SET);

    /* Receive PDUs, call visitor. */
    if (net->network.vtable.lpdu_rx) {
        net->network.vtable.lpdu_rx(net);
    }
    if (visit) pdunet_visit(net, range, visit, data);

    /* Decode PDUs. */
    pdunet_decode_unpack(net, NULL);
    pdunet_decode_linear(net, NULL);
    pdunet_visit(net, NULL, pdunet_visit_clear_update_flag, NULL);

    /* Marshal from PDU Network to SignalVector. */
    marshal_signalmap_in(net->msm);
}


void pdunet_visit_clear_update_flag(
    PduNetworkDesc* net, PduObject* pdu, void* data)
{
    UNUSED(net);
    UNUSED(data);
    if (pdu) pdu->update_signals = false;
}


void pdunet_visit_clear_tx_flag(PduNetworkDesc* net, PduObject* pdu, void* data)
{
    UNUSED(net);
    UNUSED(data);
    if (pdu) pdu->needs_tx = false;
}


void pdunet_visit_clear_checksum(
    PduNetworkDesc* net, PduObject* pdu, void* data)
{
    UNUSED(net);
    UNUSED(data);
    if (pdu) pdu->checksum = 0;
}


void pdunet_visit_needs_tx(PduNetworkDesc* net, PduObject* pdu, void* data)
{
    UNUSED(net);
    UNUSED(data);
    if (pdu == NULL || pdu->pdu == NULL) return;

    if (pdu->pdu->dir == PduDirectionTx) {
        uint32_t checksum = pdunet_checksum(
            pdu->ncodec.pdu.payload, pdu->ncodec.pdu.payload_len);
        if (checksum != pdu->checksum) {
            pdu->needs_tx = true;
            pdu->checksum = checksum;
        }
    } else {
        pdu->needs_tx = false;
    }
}


/**
pdunet_destroy
==============

Parameters
----------
net (PduNetworkDesc*)
: PduNetworkDesc object.
*/
void pdunet_destroy(PduNetworkDesc* net)
{
    if (net) {
        for (size_t i = 0; i < vector_len(&net->pdus); i++) {
            PduItem* pdu = vector_at(&net->pdus, i, NULL);
            vector_reset(&pdu->signals);
            if (pdu->metadata.config) free(pdu->metadata.config);
        }
        vector_reset(&net->pdus);
        marshal_signalmap_destroy(net->msm);
        pdunet_matrix_clear(net);
        pdunet_lua_teardown(net);
        if (net->network.metadata.config) free(net->network.metadata.config);
        free(net);
    }
}
