// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <dse/clib/collections/vector.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/schema.h>
#include <dse/ncodec/codec.h>
#include <dse/pdunet/pdunet.h>
#include <dse/pdunet/network/network.h>


#define UNUSED(x) ((void)x)

// TODO: remove when transitioning from logger-h -> log.h
extern uint8_t __log_level__;


static int _network_match_handler(ModelInstanceSpec* mi, SchemaObject* object)
{
    UNUSED(mi);

    YamlNode** doc_ref = (YamlNode**)object->data;
    *doc_ref = object->doc;

    return 1;
}

PduNetwork* model_pdunet_setup(SimulationSpec* sim, ModelInstanceSpec* mi,
    void* ncodec, SchemaLabel* net_labels, SchemaLabel* sg_labels)
{
    assert(sim);
    assert(mi);
    assert(mi->private);

    // TODO: log migration remove this log setup.
    mi->log.level = __log_level__;
    mi->log.function = dse_log2console;
    DseLog* log = (DseLog*)&mi->log;

    ModelInstancePrivate* mip = mi->private;
    lua_State*            L = mip->lua_state;
    int                   rc = 0;

    if (ncodec == NULL || net_labels == NULL || sg_labels == NULL) {
        log_debug(log, "Call without required parameters, unexpected");
        return NULL;
    }

    /* Locate the Network. */
    YamlNode* network_doc = NULL;
    size_t    net_label_count = 0;

    log_notice(log, "PDU Net: Search for Network");
    for (SchemaLabel* l = net_labels; l->name; l++) {
        log_notice(log, "  Label %s=%s", l->name, l->value);
        net_label_count++;
    }
    SchemaObjectSelector net_selector = {
        .kind = "Network",
        .labels = net_labels,
        .labels_len = net_label_count,
        .data = &network_doc,
    };
    rc = schema_object_search(mi, &net_selector, _network_match_handler);
    if (rc == 0) {
        if (network_doc == NULL) {
            log_error(log, "Search failed: no document identified");
            return NULL;
        }
    } else if (rc == -ENODATA) {
        log_fatal(
            log, "Search failed: Network not found in YAML files (rc=%d)", rc);
    } else {
        log_fatal(log, "Search failed: rc=%d", rc);
        return NULL;
    }

    /* Create the Network. */
    PduNetwork* net =
        pdunet_create(ncodec, network_doc, sim->step_size, L, log);
    if (net == NULL) {
        return NULL;
    }
    net->default_log.level = __log_level__;  // Adjust the log level manually.

    /* Locate the Signal Vector (for mapping). */
    SignalVector* net_sv = NULL;
    SchemaLabel*  channel = NULL;
    log_notice(log, "PDU Net: Search for SignalGroup (Network=%s)", net->name);
    for (SchemaLabel* l = sg_labels; l->name; l++) {
        log_debug(log, "  Label %s=%s", l->name, l->value);
        if (strcmp(l->name, "channel") == 0) {
            log_notice(log, "  Label %s=%s", l->name, l->value);
            channel = l;
            for (SignalVector* sv = mi->model_desc->sv; sv && sv->name; sv++) {
                if (strcmp(sv->alias, channel->value) == 0) {
                    net_sv = sv;
                }
            }
        }
    }
    if (channel == NULL) {
        log_error(log, "SignalGroup with annotation 'channel' not found!");
        pdunet_destroy(net);
        return NULL;
    } else if (net_sv == NULL) {
        log_error(log, "SignalVector with alias/name not found!");
        pdunet_destroy(net);
        return NULL;
    }

    /* Map the SignalVector and Network. */
    rc = pdunet_map_signals(
        net, net_sv->alias, net_sv->count, net_sv->signal, net_sv->scalar);
    if (rc != 0) {
        pdunet_destroy(net);
        return NULL;
    }

    /* Return the configured PDUNet. */
    return net;
}


PduNetwork* pdunet_find(ModelInstanceSpec* mi, void* ncodec)
{
    if (mi == NULL) return NULL;
    if (ncodec == NULL) return NULL;

    ModelInstancePrivate* mip = mi->private;
    if (mip) {
        for (size_t i = 0; i < vector_len(&mip->pdunet); i++) {
            PduNetwork* net = NULL;
            vector_at(&mip->pdunet, i, &net);
            if (net) return net;
        }
    }
    return NULL;
}
