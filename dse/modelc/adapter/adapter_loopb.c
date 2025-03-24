// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/clib/collections/set.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/private.h>
#include <dse/modelc/adapter/simbus/simbus.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/runtime.h>


#define UNUSED(x) ((void)x)


typedef enum AdapterState {
    ADAPTER_STATE_NONE = 0,
    ADAPTER_STATE_READY,
    ADAPTER_STATE_START,
    __ADAPTER_STATE_SIZE__,
} AdapterState;


typedef struct AdapterLoopbVTable {
    AdapterVTable vtable;

    /* Supporting data objects. */
    double       step_size;
    HashMap      channels;  // map{name:SimbusChannel}
    AdapterState state;
} AdapterLoopbVTable;


static SimbusChannel* _get_simbus_channel(
    AdapterLoopbVTable* v, const char* name)
{
    assert(v);
    SimbusChannel* sc = hashmap_get(&v->channels, name);
    if (sc) return sc;

    /* Allocate a new Simbus Channel.  */
    sc = calloc(1, sizeof(SimbusChannel));
    sc->name = name;
    set_init(&sc->signals);
    hashmap_set(&v->channels, name, sc);
    return sc;
}


static int _destroy_vector(void* _sc, void* _)
{
    UNUSED(_);
    SimbusChannel* sc = _sc;

    for (uint32_t i = 0; i < sc->vector.count; i++) {
        free(sc->vector.signal[i]);
        free(sc->vector.binary[i]);
    }
    free(sc->vector.signal);
    free(sc->vector.uid);
    free(sc->vector.scalar);
    free(sc->vector.binary);
    free(sc->vector.length);
    free(sc->vector.buffer_size);
    sc->vector.count = 0;
    sc->vector.signal = NULL;
    sc->vector.uid = NULL;
    sc->vector.scalar = NULL;
    sc->vector.binary = NULL;
    sc->vector.length = NULL;
    sc->vector.buffer_size = NULL;

    hashmap_destroy(&sc->vector.index);

    return 0;
}

static void _destroy_channel(void* map_item, void* data)
{
    UNUSED(data);

    SimbusChannel* sc = map_item;
    if (sc) {
        _destroy_vector(sc, NULL);
        set_destroy(&sc->signals);
    }
}

static void _destroy_vectors(AdapterLoopbVTable* v)
{
    assert(v);
    hashmap_iterator(&v->channels, _destroy_vector, false, NULL);
}


static int _generate_vector(void* _sc, void* _)
{
    UNUSED(_);
    SimbusChannel* sc = _sc;
    uint64_t       size;

    /* Allocate vector storage. */
    sc->vector.signal = set_to_array(&sc->signals, &size);
    sc->vector.count = size;
    sc->vector.uid = calloc(size, sizeof(uint32_t));
    sc->vector.binary = calloc(size, sizeof(void*));
    sc->vector.length = calloc(size, sizeof(uint32_t));
    sc->vector.buffer_size = calloc(size, sizeof(uint32_t));
    sc->vector.scalar = calloc(size, sizeof(double));

    /* Calculate UIDs. */
    for (uint32_t i = 0; i < sc->vector.count; i++) {
        // FNV-1a hash (http://www.isthe.com/chongo/tech/comp/fnv/)
        size_t   len = strlen(sc->vector.signal[i]);
        uint32_t h = 2166136261UL; /* FNV_OFFSET 32 bit */
        for (size_t j = 0; j < len; ++j) {
            h = h ^ (unsigned char)sc->vector.signal[i][j];
            h = h * 16777619UL; /* FNV_PRIME 32 bit */
        }
        sc->vector.uid[i] = h;
    }
    /* Generate the Index. */
    hashmap_init(&sc->vector.index);
    for (uint32_t i = 0; i < sc->vector.count; i++) {
        uint32_t* ptr = (uint32_t*)malloc(sizeof(uint32_t));
        *ptr = i;
        hashmap_set_alt(&sc->vector.index, sc->vector.signal[i], (void*)ptr);
    }

    return 0;
}

static void _regenerate_vectors(AdapterLoopbVTable* v)
{
    assert(v);

    /* Currently destructive (vectors are reallocated, content/values lost). */
    _destroy_vectors(v);
    hashmap_iterator(&v->channels, _generate_vector, false, NULL);
}


static void simbus_register_channels(AdapterModel* am)
{
    Adapter*            adapter = am->adapter;
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    for (uint32_t ch_idx = 0; ch_idx < am->channels_length; ch_idx++) {
        Channel*       ch = _get_channel_byindex(am, ch_idx);
        SimbusChannel* sc = _get_simbus_channel(v, ch->name);
        assert(sc);

        _refresh_index(ch);
        for (uint32_t i = 0; i < ch->index.count; i++) {
            SignalValue* sv = _get_signal_value_byindex(ch, i);
            if (sv == NULL) continue;
            if (sv->name == NULL) continue;
            set_add(&sc->signals, sv->name);
        }
        _invalidate_index(ch);
    }

    _regenerate_vectors(v);
}


static int adapter_loopb_connect(AdapterModel* am, SimulationSpec* sim, int _)
{
    UNUSED(_);
    Adapter*            adapter = am->adapter;
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    v->step_size = sim->step_size;
    hashmap_init(&v->channels);

    return 0;
}


static int adapter_loopb_register(AdapterModel* am)
{
    Adapter*            adapter = am->adapter;
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    simbus_register_channels(am);

    for (uint32_t ch_idx = 0; ch_idx < am->channels_length; ch_idx++) {
        Channel*       ch = _get_channel_byindex(am, ch_idx);
        SimbusChannel* sc = _get_simbus_channel(v, ch->name);
        assert(sc);
        log_simbus("SignalIndex <-- [%s]", ch->name);

        _refresh_index(ch);
        for (uint32_t i = 0; i < ch->index.count; i++) {
            SignalValue* sv = _get_signal_value_byindex(ch, i);
            if (sv == NULL) continue;
            if (sv->name == NULL) continue;
            uint32_t* sc_index = hashmap_get(&sc->vector.index, sv->name);
            if (sc_index == NULL) continue;

            // Cache these values make the main loops faster by avoiding
            // additional hash_get() calls.
            sv->vector_index = *sc_index;
            sv->uid = sc->vector.uid[*sc_index];
            log_simbus("    SignalLookup: %s [UID=%u]", sv->name, sv->uid);
        }
    }

    return 0;
}


static int _resolve_bus(void* _sc, void* _)
{
    UNUSED(_);
    SimbusChannel* sc = _sc;

    if (sc->is_binary) {
        for (uint32_t i = 0; i < sc->vector.count; i++) {
            sc->vector.length[i] = 0;
        }
    }

    return 0;
}


static int ready_update_sv(void* value, void* data)
{
    UNUSED(data);
    AdapterModel*       am = value;
    Adapter*            adapter = am->adapter;
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    log_simbus("Notify/ModelReady --> [...]");
    log_simbus("    model_time=%f", am->model_time);

    for (uint32_t ch_idx = 0; ch_idx < am->channels_length; ch_idx++) {
        Channel*       ch = _get_channel_byindex(am, ch_idx);
        SimbusChannel* sc = _get_simbus_channel(v, ch->name);
        assert(sc);
        log_simbus("SignalVector --> [%s]", ch->name);

        _refresh_index(ch);
        if (sc->is_binary) {
            for (uint32_t i = 0; i < ch->index.count; i++) {
                SignalValue* sv = _get_signal_value_byindex(ch, i);
                assert(sv);
                if (sv == NULL) continue;
                if (sv->name == NULL) continue;

                if (sv->bin && sv->bin_size) {
                    dse_buffer_append(&sc->vector.binary[sv->vector_index],
                        &sc->vector.length[sv->vector_index],
                        &sc->vector.buffer_size[sv->vector_index], sv->bin,
                        sv->bin_size);
                    log_simbus(
                        "    SignalValue: %u = <binary> (len=%u) [name=%s]",
                        sv->uid, sv->bin_size, sv->name);
                    /* Indicate the binary object was consumed. */
                    sv->bin_size = 0;
                }
            }
        } else {
            /* Merge in changed signals. */
            for (uint32_t i = 0; i < ch->index.count; i++) {
                SignalValue* sv = _get_signal_value_byindex(ch, i);
                if (sv->val != sv->final_val) {
                    sc->vector.scalar[sv->vector_index] = sv->final_val;
                    log_simbus("    SignalValue: %u = %f [name=%s]", sv->uid,
                        sv->final_val, sv->name);
                }
            }
        }
    }

    return 0;
}


static int adapter_loopb_model_ready(Adapter* adapter)
{
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    if (v->state != ADAPTER_STATE_READY) {
        v->state = ADAPTER_STATE_READY;
        hashmap_iterator(&v->channels, _resolve_bus, false, NULL);
    }

    hashmap_iterator(&adapter->models, ready_update_sv, true, NULL);
    return 0;
}


static int notify_update_sv(void* value, void* data)
{
    UNUSED(data);
    AdapterModel*       am = value;
    Adapter*            adapter = am->adapter;
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    /* Progress time. */
    am->stop_time = am->model_time + v->step_size;
    log_simbus("Notify/ModelStart <-- [%u]", am->model_uid);
    log_simbus("    model_uid=%u", am->model_uid);
    log_simbus("    model_time=%f", am->model_time);
    log_simbus("    stop_time=%f", am->stop_time);

    for (uint32_t ch_idx = 0; ch_idx < am->channels_length; ch_idx++) {
        Channel*       ch = _get_channel_byindex(am, ch_idx);
        SimbusChannel* sc = _get_simbus_channel(v, ch->name);
        assert(sc);
        log_simbus("SignalVector <-- [%s]", ch->name);

        _refresh_index(ch);
        if (sc->is_binary) {
            for (uint32_t i = 0; i < ch->index.count; i++) {
                SignalValue* sv = _get_signal_value_byindex(ch, i);
                assert(sv);
                if (sv == NULL) continue;
                if (sv->name == NULL) continue;

                if (sc->vector.binary[sv->vector_index] &&
                    sc->vector.length[sv->vector_index]) {
                    dse_buffer_append(&sv->bin, &sv->bin_size,
                        &sv->bin_buffer_size,
                        sc->vector.binary[sv->vector_index],
                        sc->vector.length[sv->vector_index]);
                    log_simbus(
                        "    SignalValue: %u = <binary> (len=%u) [name=%s]",
                        sv->uid, sv->bin_size, sv->name);
                }
            }
        } else {
            if (__log_level__ <= LOG_SIMBUS) {
                for (uint32_t i = 0; i < ch->index.count; i++) {
                    SignalValue* sv = _get_signal_value_byindex(ch, i);
                    if (sv->val != sc->vector.scalar[sv->vector_index]) {
                        log_simbus("    SignalValue: %u = %f [name=%s]",
                            sv->uid, sc->vector.scalar[sv->vector_index],
                            sv->name);
                    }
                }
            }

            for (uint32_t i = 0; i < ch->index.count; i++) {
                SignalValue* sv = _get_signal_value_byindex(ch, i);
                sv->final_val = sv->val = sc->vector.scalar[sv->vector_index];
            }
        }
    }

    return 0;
}

static int adapter_loopb_model_start(Adapter* adapter)
{
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    if (v->state != ADAPTER_STATE_START) {
        v->state = ADAPTER_STATE_START;
    }

    hashmap_iterator(&adapter->models, notify_update_sv, true, NULL);
    return 0;
}


void adapter_loopb_destroy(Adapter* adapter)
{
    assert(adapter);
    if (adapter->vtable == NULL) return;

    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;
    hashmap_destroy_ext(&v->channels, _destroy_channel, NULL);
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
    v->vtable.destroy = adapter_loopb_destroy;

    return (AdapterVTable*)v;
}


/*
Simbus Interfaces
-----------------
*/

extern Controller* controller_object_ref(void);


SimbusVectorIndex simbus_vector_lookup(
    SimulationSpec* sim, const char* vname, const char* sname)
{
    assert(sim);
    Controller* controller = controller_object_ref();
    assert(controller);
    assert(controller->adapter);
    if (controller->adapter->vtable == NULL) return (SimbusVectorIndex){};
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)controller->adapter->vtable;

    SimbusVectorIndex index = {};
    SimbusChannel*    sc = _get_simbus_channel(v, vname);
    if (sc) {
        uint32_t* vi = hashmap_get(&sc->vector.index, sname);
        if (vi) {
            index.sbv = &sc->vector;
            index.vi = *vi;
        }
    }
    return index;
}


static int _binary_reset(void* map_item, void* data)
{
    UNUSED(data);

    SimbusChannel* sc = map_item;
    if (sc) {
        for (uint32_t i = 0; i < sc->vector.count; i++) {
            sc->vector.length[i] = 0;
        }
    }
    return 0;
}

void simbus_vector_binary_reset(SimulationSpec* sim)
{
    assert(sim);
    Controller* controller = controller_object_ref();
    assert(controller);
    assert(controller->adapter);
    if (controller->adapter->vtable == NULL) return;

    AdapterLoopbVTable* v = (AdapterLoopbVTable*)controller->adapter->vtable;
    hashmap_iterator(&v->channels, _binary_reset, false, NULL);
}
