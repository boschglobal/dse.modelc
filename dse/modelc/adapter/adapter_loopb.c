// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/clib/collections/set.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/collections/vector.h>
#include <dse/clib/util/strings.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/private.h>
#include <dse/modelc/adapter/simbus/simbus.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/schema.h>
#include <dse/modelc/runtime.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define DIRECT_INDEX_MAP_ITEM_SIZE                                             \
    (sizeof(double) + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t))


typedef enum AdapterState {
    ADAPTER_STATE_NONE = 0,
    ADAPTER_STATE_READY,
    ADAPTER_STATE_START,
    __ADAPTER_STATE_SIZE__,
} AdapterState;


typedef struct AdapterLoopbVTable {
    AdapterVTable vtable;

    /* Configured state - indicates connect() has been called. */
    bool configured;

    /* Supporting data objects. */
    double       step_size;
    HashMap      channels;  // map{name:SimbusChannel}
    AdapterState state;

    /* Direct Indexing. */
    struct {
        bool   active;
        void*  spec;
        void*  map;
        size_t map_size;
    } direct_index;
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


static int _destroy_vector(void* _sc, void* _v)
{
    SimbusChannel*      sc = _sc;
    AdapterLoopbVTable* v = _v;

    for (uint32_t i = 0; i < sc->vector.count; i++) {
        free(sc->vector.signal[i]);
        free(sc->vector.binary[i]);
    }
    free(sc->vector.signal);
    free(sc->vector.uid);

    if (v && v->direct_index.active == true) {
        /* Direct Index memory is allocated elsewhere. */
    } else {
        free(sc->vector.scalar);
        free(sc->vector.binary);
        free(sc->vector.length);
        free(sc->vector.buffer_size);
    }
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


static void _destroy_signal_list_item(void* item, void* data)
{
    UNUSED(data);
    char* _item = *(char**)item;
    free(_item);
    _item = NULL;
}


static void _destroy_channel(void* map_item, void* _v)
{
    AdapterLoopbVTable* v = _v;
    SimbusChannel*      sc = map_item;
    if (sc) {
        _destroy_vector(sc, v);
        set_destroy(&sc->signals);
        vector_clear(
            &sc->signal_list, _destroy_signal_list_item, &sc->signal_list);
        vector_reset(&sc->signal_list);
    }
}


static void _destroy_vectors(AdapterLoopbVTable* v)
{
    assert(v);
    if (v->direct_index.active) return;
    hashmap_iterator(&v->channels, _destroy_vector, false, NULL);
}


static int _update_vector_map_refs(void* _sc, void* _v)
{
    SimbusChannel*      sc = _sc;
    AdapterLoopbVTable* v = _v;

    /* This function is called after the map is reallocated. Only the
    references should need to be updated, the content will have been
    duplicated during the realloc. */

    if (v && v->direct_index.active) {
        /* Assign vector map references. */
        void* map_offset = v->direct_index.map + sc->offset;
        sc->vector.scalar = map_offset;
        sc->vector.binary = map_offset + sc->length * 8;
        sc->vector.length = map_offset + sc->length * 16;
        sc->vector.buffer_size = map_offset + sc->length * 20;
        log_trace("  Vector::map=%p(offset=%u): scalar=%p, binary=%p, "
                  "scalar=%p, scalar=%p",
            v->direct_index.map, sc->offset, sc->vector.scalar,
            sc->vector.binary, sc->vector.length, sc->vector.buffer_size);
    }

    return 0;
}


static int _generate_vector(void* _sc, void* _v)
{
    SimbusChannel*      sc = _sc;
    AdapterLoopbVTable* v = _v;

    if (v && v->direct_index.active) {
        /* Allocate vector storage. */
        sc->vector.signal = calloc(sc->length, sizeof(char*));
        for (size_t i = 0; i < sc->length; i++) {
            sc->vector.signal[i] =
                strdup(*(char**)vector_at(&sc->signal_list, i, NULL));
        }
        sc->vector.count = sc->length;
        sc->vector.uid = calloc(sc->length, sizeof(uint32_t));
        /* Assign vector map references. */
        void* map_offset = v->direct_index.map + sc->offset;
        sc->vector.scalar = map_offset;
        sc->vector.binary = map_offset + sc->length * 8;
        sc->vector.length = map_offset + sc->length * 16;
        sc->vector.buffer_size = map_offset + sc->length * 20;
        log_trace("  Vector::map=%p(offset=%u): scalar=%p, binary=%p, "
                  "scalar=%p, scalar=%p",
            v->direct_index.map, sc->offset, sc->vector.scalar,
            sc->vector.binary, sc->vector.length, sc->vector.buffer_size);
    } else {
        /* Allocate vector storage. */
        uint64_t size;
        sc->vector.signal = set_to_array(&sc->signals, &size);
        sc->vector.count = size;
        sc->vector.uid = calloc(size, sizeof(uint32_t));
        sc->vector.scalar = calloc(size, sizeof(double));
        sc->vector.binary = calloc(size, sizeof(void*));
        sc->vector.length = calloc(size, sizeof(uint32_t));
        sc->vector.buffer_size = calloc(size, sizeof(uint32_t));
    }

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
    if (v->direct_index.active) return; /* Vectors are static in this case. */

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

        if (v->direct_index.active) {
            /* The channel is already configured/allocated. Only need to
            invalidate the index so that the caller knows to re-index. */
            _invalidate_index(ch);
            continue;
        }

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


static void* _direct_index_signal_generator(ModelInstanceSpec* mi, void* data)
{
    UNUSED(mi);

    YamlNode* n = dse_yaml_find_node((YamlNode*)data, "signal");
    if (n && n->scalar) {
        char*       signal_name = strdup(n->scalar);
        const char* _di = NULL;
        dse_yaml_get_string(data, "annotations/direct_index", &_di);
        log_trace("  %s (direct_index = %u)", signal_name, _di);
        return (signal_name);
    }
    return NULL;
}


static int _direct_index_configure(ModelInstanceSpec* mi, SchemaObject* o)
{
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)o->data;

    const char* name = NULL;
    uint32_t    offset = 0;
    uint32_t    length = 0;

    if (dse_yaml_get_string(o->doc, "metadata/name", &name)) {
        log_error("Direct Index metadata missing: metadata/name");
        return 0;
    }
    if (dse_yaml_get_uint(
            o->doc, "metadata/annotations/direct_index/offset", &offset)) {
        log_error("Direct Index annotation missing: direct_index/offset");
        return 0;
    }
    if (dse_yaml_get_uint(
            o->doc, "metadata/annotations/direct_index/length", &length)) {
        log_error("Direct Index annotation missing: direct_index/length");
        return 0;
    }

    /* Enumerate the list of signals. */
    uint32_t index = 0;
    Vector   signal_list = vector_make(sizeof(char*), 0, NULL);
    do {
        char* s = schema_object_enumerator(
            mi, o, "spec/signals", &index, _direct_index_signal_generator);
        if (s == NULL) break;
        vector_push(&signal_list, &s);
    } while (1);
    if (index != length) {
        log_error("Direct Index length mismatch: expected=%u, actual=%u",
            length, index);
        vector_clear(&signal_list, _destroy_signal_list_item, NULL);
        vector_reset(&signal_list);
        return 0;
    }

    /* At this point indicate that direct_index is in use. */
    v->direct_index.active = true;

    /* Allocate/configure the direct index. */
    size_t map_offset = offset * DIRECT_INDEX_MAP_ITEM_SIZE;
    size_t map_len = length * DIRECT_INDEX_MAP_ITEM_SIZE;
    if (v->direct_index.map_size < (map_offset + map_len)) {
        /* Realloc the map. */
        size_t old_map_size = v->direct_index.map_size;
        v->direct_index.map_size = map_offset + map_len;
        v->direct_index.map =
            realloc(v->direct_index.map, v->direct_index.map_size);
        log_debug("Map Allocate: map=%p size=%u (offset=%u, len=%u)",
            v->direct_index.map, v->direct_index.map_size, map_offset, map_len);
        /* Clear the newly allocated part of the map. */
        memset(v->direct_index.map + old_map_size, 0,
            v->direct_index.map_size - old_map_size);
        /* Update existing vectors (to point to relocated map). */
        hashmap_iterator(&v->channels, _update_vector_map_refs, true, v);
    }

    /* Create the SimbusChannel object. */
    SimbusChannel* sc = _get_simbus_channel(v, name);
    assert(sc);
    sc->signal_list = signal_list;
    sc->offset = map_offset;
    sc->length = length;
    _generate_vector(sc, v);

    /* Continue with next match. */
    return 0;
}


static int adapter_loopb_connect(AdapterModel* am, SimulationSpec* sim, int _)
{
    UNUSED(_);
    assert(am);
    assert(am->adapter);
    assert(am->adapter->vtable);
    Adapter*            adapter = am->adapter;
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    if (v->configured) return 0; /* Only need to configure this object once. */

    v->step_size = sim->step_size;
    hashmap_init(&v->channels);

    /* Direct Indexing - determine if a direct index is configured. */
    if (sim->instance_list == NULL) {
        log_error("Unexpected condition, sim object has no ModelInstances");
        return 0;
    }
    SchemaLabel scalar_v_labels[] = {
        { .name = "index", .value = "direct" },
    };
    SchemaObjectSelector scalar_v_sel = {
        .kind = "SignalGroup",
        .labels = scalar_v_labels,
        .labels_len = ARRAY_SIZE(scalar_v_labels),
        .data = v,
    };
    /* Use the YAML doc from first model (same for all). */
    schema_object_search(
        sim->instance_list, &scalar_v_sel, _direct_index_configure);

    v->configured = true;
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
        if (sc->vector.index.hash_function == NULL) {
            log_fatal("SimBus Channel not initialised, index missing, mismatch "
                      "with Channel");
        }

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

    for (uint32_t i = 0; i < sc->vector.count; i++) {
        sc->vector.length[i] = 0;
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
                log_simbus("    SignalValue: %u = <binary> (len=%u) [name=%s]",
                    sv->uid, sv->bin_size, sv->name);
                /* Indicate the binary object was consumed. */
                sv->bin_size = 0;
            } else if (sv->val != sv->final_val) {
                sc->vector.scalar[sv->vector_index] = sv->final_val;
                log_simbus("    SignalValue: %u = %f [name=%s]", sv->uid,
                    sv->final_val, sv->name);
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
        for (uint32_t i = 0; i < ch->index.count; i++) {
            SignalValue* sv = _get_signal_value_byindex(ch, i);
            assert(sv);
            if (sv == NULL) continue;
            if (sv->name == NULL) continue;

            if (sc->vector.binary[sv->vector_index] &&
                sc->vector.length[sv->vector_index]) {
                dse_buffer_append(&sv->bin, &sv->bin_size, &sv->bin_buffer_size,
                    sc->vector.binary[sv->vector_index],
                    sc->vector.length[sv->vector_index]);
                log_simbus("    SignalValue: %u = <binary> (len=%u) [name=%s]",
                    sv->uid, sv->bin_size, sv->name);
            } else {
                if (__log_level__ <= LOG_SIMBUS) {
                    if (sv->val != sc->vector.scalar[sv->vector_index]) {
                        log_simbus("    SignalValue: %u = %f [name=%s]",
                            sv->uid, sc->vector.scalar[sv->vector_index],
                            sv->name);
                    }
                }

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
    hashmap_destroy_ext(&v->channels, _destroy_channel, v);
    if (v && v->direct_index.active) {
        free(v->direct_index.map);
        v->direct_index.map = NULL;
    }
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
SimBus Interfaces
-----------------
*/

SimbusVectorIndex simbus_vector_lookup(
    SimulationSpec* sim, const char* vname, const char* sname)
{
    /* Default return value is empty index object. */
    SimbusVectorIndex index = {};

    assert(sim);
    Controller* controller = controller_object_ref(sim);
    assert(controller);
    assert(controller->adapter);

    if (controller->adapter->vtable == NULL) return index;
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)controller->adapter->vtable;
    SimbusChannel*      sc = _get_simbus_channel(v, vname);
    if (sc) {
        index.direct_index.map = v->direct_index.map;
        index.direct_index.offset = sc->offset;
        index.direct_index.size = v->direct_index.map_size;
        if (sname) {
            /* Search for the signal. */
            uint32_t* vi = hashmap_get(&sc->vector.index, sname);
            if (vi) {
                index.sbv = &sc->vector;
                index.vi = *vi;
            }
        } else {
            /* Vector lookup only, return the vector address. */
            index.sbv = &sc->vector;
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
    Controller* controller = controller_object_ref(sim);
    assert(controller);
    assert(controller->adapter);
    if (controller->adapter->vtable == NULL) return;

    AdapterLoopbVTable* v = (AdapterLoopbVTable*)controller->adapter->vtable;
    hashmap_iterator(&v->channels, _binary_reset, false, NULL);
}
