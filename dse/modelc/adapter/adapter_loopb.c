// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/private.h>
#include <dse/modelc/runtime.h>


#define UNUSED(x) ((void)x)


typedef struct AdapterLoopbVTable {
    AdapterVTable vtable;

    /* Supporting data objects. */
    double step_size;
} AdapterLoopbVTable;


static int adapter_loopb_connect(AdapterModel* am, SimulationSpec* sim, int _)
{
    UNUSED(_);
    Adapter*            adapter = am->adapter;
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    v->step_size = sim->step_size;
    return 0;
}


static int adapter_loopb_register(AdapterModel* am)
{
    for (uint32_t ch_idx = 0; ch_idx < am->channels_length; ch_idx++) {
        Channel* ch = _get_channel_byindex(am, ch_idx);
        log_simbus("SignalIndex <-- [%s]", ch->name);

        _refresh_index(ch);
        for (uint32_t i = 0; i < ch->index.count; i++) {
            SignalValue* sv = _get_signal_value_byindex(ch, i);
            if (sv == NULL) continue;
            if (sv->name == NULL) continue;
            {
                // FNV-1a hash (http://www.isthe.com/chongo/tech/comp/fnv/)
                size_t   len = strlen(sv->name);
                uint32_t h = 2166136261UL; /* FNV_OFFSET 32 bit */
                for (size_t i = 0; i < len; ++i) {
                    h = h ^ (unsigned char)sv->name[i];
                    h = h * 16777619UL; /* FNV_PRIME 32 bit */
                }
                sv->uid = h;
            }
            log_simbus("    SignalLookup: %s [UID=%u]", sv->name, sv->uid);
        }
    }

    return 0;
}


static int adapter_loopb_model_ready(AdapterModel* am)
{
    log_simbus("Notify/ModelReady --> [...]");
    log_simbus("    model_time=%f", am->model_time);
    return 0;
}


static int adapter_loopb_model_start(AdapterModel* am)
{
    Adapter*            adapter = am->adapter;
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    /* Progress time. */
    am->stop_time = am->model_time + v->step_size;

    log_simbus("Notify/ModelStart <-- [%u]", am->model_uid);
    log_simbus("    model_uid=%u", am->model_uid);
    log_simbus("    model_time=%f", am->model_time);
    log_simbus("    stop_time=%f", am->stop_time);

    for (uint32_t ch_idx = 0; ch_idx < am->channels_length; ch_idx++) {
        Channel* ch = _get_channel_byindex(am, ch_idx);
        log_simbus("SignalVector <-- [%s]", ch->name);

        _refresh_index(ch);
        for (uint32_t i = 0; i < ch->index.count; i++) {
            SignalValue* sv = _get_signal_value_byindex(ch, i);
            if (sv == NULL) continue;
            if (sv->bin) {
                /* Binary. */
                if (sv->bin_size) {
                    log_simbus(
                        "    SignalValue: %u = <binary> (len=%u) [name=%s]",
                        sv->uid, sv->bin_size, sv->name);
                }
            } else {
                /* Double. */
                if (sv->val != sv->final_val) {
                    sv->val = sv->final_val;
                    log_simbus("    SignalValue: %u = %f [name=%s]", sv->uid,
                        sv->val, sv->name);
                }
            }
        }
    }

    return 0;
}


/*
Adapter VTable Create
---------------------
*/

AdapterVTable* adapter_create_loopb_vtable(void)
{
    AdapterLoopbVTable* v = calloc(1, sizeof(AdapterLoopbVTable));

    /* Adapter interface. */
    v->vtable.connect = adapter_loopb_connect;
    v->vtable.register_ = adapter_loopb_register;
    v->vtable.ready = adapter_loopb_model_ready;
    v->vtable.start = adapter_loopb_model_start;

    return (AdapterVTable*)v;
}
