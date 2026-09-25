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

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define LIKELY(x)     __builtin_expect(!!(x), 1)
#define UNLIKELY(x)   __builtin_expect(!!(x), 0)
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
    Vector       channels;  // sorted SimbusChannel* by name
    AdapterState state;

    /* Reset on every model (re)registration; triggers a subscriber
       rebuild and a fresh initial-conditions push. */
    bool scalar_models_initialized;
    bool scalar_initial_push_done;

    /* Direct Indexing. */
    struct {
        bool   active;
        void*  spec;
        void*  map;
        size_t map_size;
    } direct_index;
} AdapterLoopbVTable;


static SimbusChannel* _get_simbus_channel(
    AdapterLoopbVTable* v, const char* name);


static inline void flatmap_mark_changed(SimbusVector* vector, uint32_t index)
{
    uint32_t* changed_generation = vector->changed_generation;
    uint32_t* changed_indices = vector->changed_indices;
    if (changed_generation[index] == vector->generation) return;
    changed_generation[index] = vector->generation;
    changed_indices[vector->changed_count++] = index;
    if (vector->changed_count >= vector->count / 2) {
        vector->dense_changes = true;
    }
}


static int flatmap_clear_tracking(void* item, void* data)
{
    UNUSED(data);
    SimbusChannel* sc = *(SimbusChannel**)item;
    sc->vector.changed_count = 0;
    sc->vector.dense_changes = false;
    if (++sc->vector.generation == 0) {
        memset(sc->vector.changed_generation, 0,
            sc->vector.count * sizeof(uint32_t));
        sc->vector.generation = 1;
    }
    return 0;
}


static void flatmap_clear_models(SimbusChannel* sc)
{
    if (sc->vector.scalar_models == NULL) return;
    for (uint32_t i = 0; i < sc->vector.count; i++) {
        vector_reset(&sc->vector.scalar_models[i]);
    }
}


static void flatmap_add_model(SimbusChannel* sc, ModelFunctionChannel* mfc,
    uint32_t index, uint32_t local)
{
    if (sc->vector.scalar_models == NULL) return;
    SimbusScalarModelRef model_ref = { .mfc = mfc, .local_index = local };
    vector_push(&sc->vector.scalar_models[index], &model_ref);
}


static int _model_signal_local_index(
    ModelFunctionChannel* mfc, const char* signal_name, uint32_t* local_index)
{
    for (uint32_t i = 0; i < mfc->signal_count; i++) {
        if (strcmp(mfc->signal_names[i], signal_name) == 0) {
            *local_index = i;
            return 0;
        }
    }
    return -1;
}


static void flatmap_build_models(AdapterLoopbVTable* v, Adapter* adapter)
{
    for (uint32_t i = 0; i < vector_len(&v->channels); i++) {
        SimbusChannel* sc = *(SimbusChannel**)vector_at(&v->channels, i, NULL);
        flatmap_clear_models(sc);
    }
    for (uint32_t mi = 0; mi < vector_len(&adapter->models); mi++) {
        AdapterModelIndexItem* model_item =
            vector_at(&adapter->models, mi, NULL);
        AdapterModel* model = model_item->am;
        for (uint32_t ci = 0; ci < vector_len(&model->channels); ci++) {
            ChannelIndexItem* channel_item =
                vector_at(&model->channels, ci, NULL);
            Channel* channel = channel_item->ch;
            if (channel->is_binary || channel->mfc == NULL) continue;
            SimbusChannel* sc = _get_simbus_channel(v, channel->name);
            _refresh_index(channel);
            ModelFunctionChannel* mfc = channel->mfc;
            if (mfc->scalar_sync.capable &&
                (mfc->scalar_sync.simbus_indices == NULL ||
                    mfc->scalar_sync.sync_count != mfc->signal_count)) {
                SignalMap* signal_map = adapter_get_signal_map(model,
                    mfc->channel_name, mfc->signal_names, mfc->signal_count);
                model_function_channel_build_flat_sync(mfc, signal_map);
                free(signal_map);
            }
            if (!mfc->scalar_sync.enabled ||
                mfc->scalar_sync.output_shadow == NULL)
                continue;
            mfc->scalar_sync.simbus_channel = sc;
            mfc->scalar_sync.simbus_scalar = sc->vector.scalar;
            for (uint32_t si = 0; si < channel->index.count; si++) {
                SignalValue* sv = channel->index.map[si].signal;
                uint32_t     local_index = 0;
                if (_model_signal_local_index(mfc, sv->name, &local_index) ==
                    0) {
                    flatmap_add_model(sc, mfc, sv->vector_index, local_index);
                }
            }
        }
    }
}


static bool flatmap_avx2_available(void)
{
#if defined(__x86_64__) || defined(__i386__)
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
}


static void flatmap_push_to_model_dense_scalar(
    ModelFunctionChannel* mfc, SimbusChannel* sc, uint32_t start)
{
    const uint32_t* indices = mfc->scalar_sync.simbus_indices;
    const double*   scalar = sc->vector.scalar;
    double*         values = mfc->signal_value_double;
    double*         shadow = mfc->scalar_sync.output_shadow;
    for (uint32_t si = start; si < mfc->signal_count; si++) {
        double value = scalar[indices[si]];
        values[si] = value;
        shadow[si] = value;
    }
}


#if defined(__x86_64__) || defined(__i386__)
__attribute__((target("avx2"))) static void flatmap_push_to_model_dense_avx2(
    ModelFunctionChannel* mfc, SimbusChannel* sc)
{
    const uint32_t  count = mfc->signal_count;
    const uint32_t* indices = mfc->scalar_sync.simbus_indices;
    const double*   scalar = sc->vector.scalar;
    double*         values = mfc->signal_value_double;
    double*         shadow = mfc->scalar_sync.output_shadow;
    uint32_t        si = 0;

    for (; si + 4 <= count; si += 4) {
        __m128i index =
            _mm_loadu_si128((const __m128i*)(const void*)&indices[si]);
        __m256d value = _mm256_i32gather_pd(scalar, index, sizeof(double));
        _mm256_storeu_pd(&values[si], value);
        _mm256_storeu_pd(&shadow[si], value);
    }
    flatmap_push_to_model_dense_scalar(mfc, sc, si);
}
#endif


static void flatmap_push_to_model_dense(
    ModelFunctionChannel* mfc, SimbusChannel* sc)
{
#if defined(__x86_64__) || defined(__i386__)
    if (flatmap_avx2_available()) {
        flatmap_push_to_model_dense_avx2(mfc, sc);
        return;
    }
#endif
    flatmap_push_to_model_dense_scalar(mfc, sc, 0);
}


static void flatmap_pull_from_models_dense_tail(
    ModelFunctionChannel* mfc, SimbusChannel* sc, uint32_t start)
{
    const uint32_t* indices = mfc->scalar_sync.simbus_indices;
    const double*   values = mfc->signal_value_double;
    const double*   shadow = mfc->scalar_sync.output_shadow;
    double*         scalar = sc->vector.scalar;
    for (uint32_t si = start; si < mfc->signal_count; si++) {
        if (shadow[si] != values[si]) {
            scalar[indices[si]] = values[si];
        }
    }
}


#if defined(__x86_64__) || defined(__i386__)
__attribute__((target("avx2"))) static void flatmap_pull_from_models_avx2(
    ModelFunctionChannel* mfc, SimbusChannel* sc)
{
    const uint32_t  count = mfc->scalar_sync.sync_count;
    const uint32_t* indices = mfc->scalar_sync.simbus_indices;
    const double*   values = mfc->signal_value_double;
    double*         shadow = mfc->scalar_sync.output_shadow;
    double*         scalar = sc->vector.scalar;
    SimbusVector*   vector = &sc->vector;
    uint32_t        si = 0;

    if (sc->vector.dense_changes) {
        flatmap_pull_from_models_dense_tail(mfc, sc, 0);
        return;
    }

    for (; si + 4 <= count; si += 4) {
        __m256d next = _mm256_loadu_pd(&values[si]);
        __m256d previous = _mm256_loadu_pd(&shadow[si]);
        int     changed =
            _mm256_movemask_pd(_mm256_cmp_pd(previous, next, _CMP_NEQ_UQ));

        if (changed == 0) continue;

        for (uint32_t lane = 0; lane < 4; lane++) {
            if (changed & (1 << lane)) {
                scalar[indices[si + lane]] = values[si + lane];
                flatmap_mark_changed(vector, indices[si + lane]);
                if (vector->dense_changes) {
                    flatmap_pull_from_models_dense_tail(mfc, sc, si + lane + 1);
                    return;
                }
            }
        }
    }
    for (; si < count; si++) {
        if (shadow[si] != values[si]) {
            scalar[indices[si]] = values[si];
            flatmap_mark_changed(vector, indices[si]);
            if (vector->dense_changes) {
                flatmap_pull_from_models_dense_tail(mfc, sc, si + 1);
                return;
            }
        }
    }
}
#endif


static bool flatmap_pull_from_models(
    ModelFunctionChannel* mfc, SimbusChannel* sc)
{
    if (mfc == NULL || sc == NULL || !mfc->scalar_sync.enabled ||
        mfc->scalar_sync.simbus_scalar != sc->vector.scalar ||
        mfc->scalar_sync.sync_count != mfc->signal_count ||
        mfc->signal_value_double == NULL ||
        mfc->scalar_sync.simbus_indices == NULL ||
        mfc->scalar_sync.output_shadow == NULL) {
        return false;
    }

    const uint32_t* indices = mfc->scalar_sync.simbus_indices;
    const double*   values = mfc->signal_value_double;
    double*         shadow = mfc->scalar_sync.output_shadow;
    SimbusVector*   vector = &sc->vector;
    if (sc->vector.dense_changes) {
        flatmap_pull_from_models_dense_tail(mfc, sc, 0);
        return true;
    }
    if (flatmap_avx2_available()) {
#if defined(__x86_64__) || defined(__i386__)
        flatmap_pull_from_models_avx2(mfc, sc);
        return true;
#endif
    }
    for (uint32_t si = 0; si < mfc->signal_count; si++) {
        if (shadow[si] != values[si]) {
            sc->vector.scalar[indices[si]] = values[si];
            flatmap_mark_changed(vector, indices[si]);
            if (vector->dense_changes) {
                flatmap_pull_from_models_dense_tail(mfc, sc, si + 1);
                return true;
            }
            if (sc->vector.dense_changes) {
                flatmap_pull_from_models_dense_tail(mfc, sc, si + 1);
                return true;
            }
        }
    }
    return true;
}


static int _compare_simbus_channel_name(const void* left, const void* right)
{
    const SimbusChannel* const* left_sc = left;
    const SimbusChannel* const* right_sc = right;

    if ((*left_sc)->name == NULL && (*right_sc)->name == NULL) return 0;
    if ((*left_sc)->name == NULL) return -1;
    if ((*right_sc)->name == NULL) return 1;
    return strcmp((*left_sc)->name, (*right_sc)->name);
}


static SimbusChannel* _get_simbus_channel(
    AdapterLoopbVTable* v, const char* name)
{
    assert(v);

    SimbusChannel* key_sc = &(SimbusChannel){ .name = name };
    SimbusChannel* item =
        VECTOR_FIND(&v->channels, SimbusChannel*, key_sc, res, {
            const SimbusChannel* left_sc = *(const SimbusChannel**)left;
            const SimbusChannel* right_sc = (const SimbusChannel*)right;

            if (left_sc->name == NULL && right_sc->name == NULL)
                res = 0;
            else if (left_sc->name == NULL)
                res = -1;
            else if (right_sc->name == NULL)
                res = 1;
            else
                res = strcmp(left_sc->name, right_sc->name);
        });
    if (item) return item;

    /* Allocate a new Simbus Channel. */
    SimbusChannel* sc = calloc(1, sizeof(SimbusChannel));
    if (UNLIKELY(!sc)) return NULL;

    sc->name = name;
    set_init(&sc->signals);
    vector_push(&v->channels, &sc);
    vector_sort(&v->channels);

    return sc;
}


static int _destroy_vector(void* _sc, void* _v)
{
    SimbusChannel*      sc = *(SimbusChannel**)_sc;
    AdapterLoopbVTable* v = _v;

    for (uint32_t i = 0; i < sc->vector.count; i++) {
        free(sc->vector.signal[i]);
        free(sc->vector.binary[i]);
    }
    free(sc->vector.signal);
    free(sc->vector.uid);
    if (sc->vector.scalar_models != NULL) {
        for (uint32_t i = 0; i < sc->vector.count; i++) {
            vector_reset(&sc->vector.scalar_models[i]);
        }
        free(sc->vector.scalar_models);
    }
    free(sc->vector.changed_indices);
    free(sc->vector.changed_generation);

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
    sc->vector.changed_indices = NULL;
    sc->vector.changed_generation = NULL;
    sc->vector.scalar_models = NULL;
    sc->vector.changed_count = 0;
    sc->vector.changed_capacity = 0;
    sc->vector.generation = 0;
    sc->vector.dense_changes = false;
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


static int _destroy_channel(void* map_item, void* _v)
{
    AdapterLoopbVTable* v = _v;
    SimbusChannel*      sc = *(SimbusChannel**)map_item;
    if (sc) {
        _destroy_vector(&sc, v);
        set_destroy(&sc->signals);
        vector_clear(
            &sc->signal_list, _destroy_signal_list_item, &sc->signal_list);
        vector_reset(&sc->signal_list);
        free(sc);
    }

    return 0;
}


static void _destroy_vectors(AdapterLoopbVTable* v)
{
    assert(v);
    if (v->direct_index.active) return;
    vector_foreach(&v->channels, _destroy_vector, NULL);
}


static int _update_vector_map_refs(void* _sc, void* _v)
{
    SimbusChannel*      sc = *(SimbusChannel**)_sc;
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
    SimbusChannel*      sc = *(SimbusChannel**)_sc;
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

    if (sc->vector.scalar_models != NULL) {
        for (uint32_t i = 0; i < sc->vector.count; i++) {
            vector_reset(&sc->vector.scalar_models[i]);
        }
        free(sc->vector.scalar_models);
    }
    free(sc->vector.changed_indices);
    free(sc->vector.changed_generation);
    sc->vector.changed_capacity = sc->vector.count;
    sc->vector.changed_indices =
        calloc(sc->vector.changed_capacity, sizeof(uint32_t));
    sc->vector.changed_generation = calloc(sc->vector.count, sizeof(uint32_t));
    sc->vector.generation = 1;
    sc->vector.dense_changes = false;
    sc->vector.scalar_models = calloc(sc->vector.count, sizeof(Vector));
    for (uint32_t i = 0; i < sc->vector.count; i++) {
        sc->vector.scalar_models[i] =
            vector_make(sizeof(SimbusScalarModelRef), 0, NULL);
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
    vector_foreach(&v->channels, _generate_vector, NULL);
}


static void simbus_register_channels(AdapterModel* am)
{
    Adapter*            adapter = am->adapter;
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    for (uint32_t ch_idx = 0; ch_idx < vector_len(&am->channels); ch_idx++) {
        Channel*       ch = _get_channel_byindex(am, ch_idx);
        SimbusChannel* sc = _get_simbus_channel(v, ch->name);
        assert(sc);
        sc->is_binary = ch->is_binary;

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
        vector_foreach(&v->channels, _update_vector_map_refs, v);
    }

    /* Create the SimbusChannel object. */
    SimbusChannel* sc = _get_simbus_channel(v, name);
    assert(sc);
    sc->signal_list = signal_list;
    sc->offset = map_offset;
    sc->length = length;
    _generate_vector(&sc, v);

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
    v->channels =
        vector_make(sizeof(SimbusChannel*), 0, _compare_simbus_channel_name);

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
    v->scalar_models_initialized = false;

    const uint32_t channel_count = vector_len(&am->channels);

    for (uint32_t ch_idx = 0; ch_idx < channel_count; ch_idx++) {
        Channel* restrict ch = _get_channel_byindex(am, ch_idx);
        SimbusChannel* restrict sc = _get_simbus_channel(v, ch->name);
        assert(sc);

        if (UNLIKELY(__log_level__ <= LOG_SIMBUS)) {
            log_simbus("SignalIndex <-- [%s]", ch->name);
        }
        if (UNLIKELY(sc->vector.index.hash_function == NULL)) {
            log_fatal("SimBus Channel not initialised, index missing, mismatch "
                      "with Channel");
        }

        _refresh_index(ch);

        if (!ch->is_binary && ch->mfc != NULL) {
            ModelFunctionChannel* mfc = ch->mfc;
            mfc->scalar_sync.simbus_channel = sc;
            mfc->scalar_sync.simbus_scalar = sc->vector.scalar;
        }

        const uint32_t signal_count = ch->index.count;
        HashMap* restrict index_map = &sc->vector.index;
        uint32_t* restrict uid_vector = sc->vector.uid;
        for (uint32_t i = 0; i < signal_count; i++) {
            SignalValue* restrict sv = _get_signal_value_byindex(ch, i);
            if (sv == NULL || sv->name == NULL) continue;

            uint32_t* sc_index = hashmap_get(index_map, sv->name);
            if (sc_index == NULL) continue;

            // Cache these values make the main loops faster by avoiding
            // additional hash_get() calls.
            const uint32_t resolved_idx = *sc_index;
            sv->vector_index = resolved_idx;
            sv->uid = uid_vector[resolved_idx];
            if (UNLIKELY(__log_level__ <= LOG_SIMBUS)) {
                log_simbus("    SignalLookup: %s [UID=%u]", sv->name, sv->uid);
            }
        }
    }

    return 0;
}


static int _resolve_bus(void* _sc, void* _)
{
    UNUSED(_);
    SimbusChannel* restrict sc = *(SimbusChannel**)_sc;

    if (sc == NULL || sc->is_binary == false || sc->vector.count == 0) {
        return 0;
    }

    uint32_t count = sc->vector.count;
    uint32_t* restrict length = sc->vector.length;

    for (uint32_t i = 0; i < count; i++) {
        length[i] = 0;
    }

    return 0;
}


static int ready_update_sv(void* value, void* data)
{
    UNUSED(data);
    AdapterModelIndexItem* item = value;
    AdapterModel*          am = item->am;
    Adapter*               adapter = am->adapter;
    AdapterLoopbVTable*    v = (AdapterLoopbVTable*)adapter->vtable;
    size_t                 ch_count = VECTOR_LEN(&am->channels);

    /* Pass 1: Logging. */
    if (UNLIKELY(__log_level__ <= LOG_SIMBUS)) {
        log_simbus("Notify/ModelReady --> [...]");
        log_simbus("    model_time=%f", am->model_time);

        for (uint32_t ch_idx = 0; ch_idx < ch_count; ch_idx++) {
            ChannelIndexItem* channel_item = VECTOR_AT(&am->channels, ch_idx);
            Channel*          ch = channel_item->ch;
            SimbusChannel*    sc = _get_simbus_channel(v, ch->name);
            assert(sc);
            _refresh_index(ch);

            log_simbus("  SignalVector --> [%s]", ch->name);

            uint32_t   index_count = ch->index.count;
            SignalMap* index_map = ch->index.map;
            for (uint32_t i = 0; i < index_count; i++) {
                SignalValue* sv = index_map[i].signal;
                assert(sv);
                assert(sv->name);

                if (sv->bin && sv->bin_size) {
                    log_simbus(
                        "    SignalValue: %u = <binary> (len=%u) [name=%s]",
                        sv->uid, sv->bin_size, sv->name);
                } else if (sv->val != sv->final_val) {
                    log_simbus("    SignalValue: %u = %f [name=%s]", sv->uid,
                        sv->final_val, sv->name);
                }
            }
        }
    }

    /* Pass 2: Execution hot-path. */
    for (uint32_t ch_idx = 0; ch_idx < ch_count; ch_idx++) {
        ChannelIndexItem* channel_item = VECTOR_AT(&am->channels, ch_idx);
        Channel*          ch = channel_item->ch;
        SimbusChannel*    sc =
            ch->mfc && !ch->is_binary
                   ? ((ModelFunctionChannel*)ch->mfc)->scalar_sync.simbus_channel
                   : NULL;
        if (sc == NULL) sc = _get_simbus_channel(v, ch->name);
        _refresh_index(ch);

        uint32_t index_count = ch->index.count;
        if (index_count == 0) continue;

        SignalMap* index_map = ch->index.map;

        if (ch->is_binary) {
            for (uint32_t i = 0; i < index_count; i++) {
                SignalValue* sv = index_map[i].signal;
                if (sv->bin_size) {
                    dse_buffer_append(&sc->vector.binary[sv->vector_index],
                        &sc->vector.length[sv->vector_index],
                        &sc->vector.buffer_size[sv->vector_index], sv->bin,
                        sv->bin_size);
                    /* Indicate the binary object was consumed. */
                    sv->bin_size = 0;
                }
            }
        } else {
            ModelFunctionChannel* mfc = ch->mfc;
            if (mfc != NULL) mfc->scalar_sync.simbus_scalar = sc->vector.scalar;
            if (flatmap_pull_from_models(mfc, sc)) continue;
            for (uint32_t i = 0; i < index_count; i++) {
                SignalValue* sv = index_map[i].signal;
                if (sv->val != sv->final_val) {
                    sc->vector.scalar[sv->vector_index] = sv->final_val;
                }
            }
        }
    }

    return 0;
}


/**
adapter_loopb_model_ready
=========================

Model -[NotifyMessage]-> SimBus

Indicate completion of model execution, model is _ready_ for next step.

Parameters
----------
adapter (Adapter*)
: Pointer to the adapter object.

Returns
-------
0 (int)
: Function completed successfully.

rc (int)
: Non-zero value indicates failure.
 */
static int adapter_loopb_model_ready(Adapter* adapter)
{
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    if (!v->scalar_models_initialized) {
        flatmap_build_models(v, adapter);
        v->scalar_models_initialized = true;
        v->scalar_initial_push_done = false;
    }

    if (v->state != ADAPTER_STATE_READY) {
        v->state = ADAPTER_STATE_READY;
        vector_foreach(&v->channels, _resolve_bus, NULL);
    }

    vector_foreach(&adapter->models, ready_update_sv, NULL);
    return 0;
}


static void flatmap_push_initial_conditions(AdapterLoopbVTable* v)
{
    if (v->scalar_initial_push_done) return;

    for (uint32_t ci = 0; ci < vector_len(&v->channels); ci++) {
        SimbusChannel* sc = *(SimbusChannel**)vector_at(&v->channels, ci, NULL);
        if (sc->vector.scalar_models == NULL) continue;
        for (uint32_t index = 0; index < sc->vector.count; index++) {
            double  value = sc->vector.scalar[index];
            Vector* model_refs = &sc->vector.scalar_models[index];
            for (uint32_t sub = 0; sub < vector_len(model_refs); sub++) {
                SimbusScalarModelRef* model_ref =
                    vector_at(model_refs, sub, NULL);
                ModelFunctionChannel* mfc = model_ref->mfc;
                mfc->signal_value_double[model_ref->local_index] = value;
                mfc->scalar_sync.output_shadow[model_ref->local_index] = value;
            }
        }
    }

    v->scalar_initial_push_done = true;
}


/**
flatmap_push_to_models
======================

Push updated scalar signals to models. Operates in two modes:

* Dense : the number of changed signals has exceeded 50% and the algorithm
switches to a simple push of all model signals (from the simbus).
* Sparse : only changed signals are pushed, based on the
`vector.changed_indices` and subscribed models (`vector.scalar_models`).


Parameters
----------
v (AdapterLoopbVTable*)
: Pointer to AdapterLoopbVTable, used for stateful information.

adapter (Adapter*)
: Pointer to the adapter object.
 */
static void flatmap_push_to_models(AdapterLoopbVTable* v, Adapter* adapter)
{
    flatmap_push_initial_conditions(v);

    for (uint32_t ci = 0; ci < vector_len(&v->channels); ci++) {
        SimbusChannel* sc = *(SimbusChannel**)vector_at(&v->channels, ci, NULL);

        if (!sc->vector.dense_changes && sc->vector.changed_count == 0)
            continue;

        if (sc->vector.dense_changes) {
            for (uint32_t mi = 0; mi < vector_len(&adapter->models); mi++) {
                AdapterModelIndexItem* model_item =
                    vector_at(&adapter->models, mi, NULL);
                AdapterModel* model = model_item->am;
                for (uint32_t mci = 0; mci < vector_len(&model->channels);
                    mci++) {
                    ChannelIndexItem* channel_item =
                        vector_at(&model->channels, mci, NULL);
                    ModelFunctionChannel* mfc = channel_item->ch->mfc;
                    if (channel_item->ch->is_binary || mfc == NULL ||
                        !mfc->scalar_sync.enabled ||
                        mfc->scalar_sync.simbus_channel != sc ||
                        mfc->scalar_sync.simbus_indices == NULL)
                        continue;
                    flatmap_push_to_model_dense(mfc, sc);
                }
            }
        } else {
            for (uint32_t si = 0; si < sc->vector.changed_count; si++) {
                uint32_t index = sc->vector.changed_indices[si];
                double   value = sc->vector.scalar[index];
                Vector*  model_refs = &sc->vector.scalar_models[index];
                for (uint32_t sub = 0; sub < vector_len(model_refs); sub++) {
                    SimbusScalarModelRef* model_ref =
                        vector_at(model_refs, sub, NULL);
                    ModelFunctionChannel* mfc = model_ref->mfc;
                    mfc->signal_value_double[model_ref->local_index] = value;
                    mfc->scalar_sync.output_shadow[model_ref->local_index] =
                        value;
                }
            }
        }
    }
}


static int notify_update_sv(void* value, void* data)
{
    UNUSED(data);
    AdapterModelIndexItem* item = value;
    AdapterModel*          am = item->am;
    Adapter*               adapter = am->adapter;
    AdapterLoopbVTable*    v = (AdapterLoopbVTable*)adapter->vtable;
    size_t                 ch_count = VECTOR_LEN(&am->channels);

    /* Progress time. */
    am->stop_time = am->model_time + v->step_size;

    /* Pass 1: Logging. */
    if (UNLIKELY(__log_level__ <= LOG_SIMBUS)) {
        log_simbus("Notify/ModelStart <-- [%u]", am->model_uid);
        log_simbus("    model_uid=%u", am->model_uid);
        log_simbus("    model_time=%f", am->model_time);
        log_simbus("    stop_time=%f", am->stop_time);

        for (uint32_t ch_idx = 0; ch_idx < ch_count; ch_idx++) {
            ChannelIndexItem* channel_item = VECTOR_AT(&am->channels, ch_idx);
            Channel*          ch = channel_item->ch;
            SimbusChannel*    sc = _get_simbus_channel(v, ch->name);
            assert(sc);
            _refresh_index(ch);

            log_simbus("SignalVector <-- [%s]", ch->name);

            uint32_t   index_count = ch->index.count;
            SignalMap* index_map = ch->index.map;
            for (uint32_t i = 0; i < index_count; i++) {
                SignalValue* sv = index_map[i].signal;
                uint32_t     v_idx = sv->vector_index;
                assert(sv);
                assert(sv->name);

                if (sc->vector.binary[v_idx] && sc->vector.length[v_idx]) {
                    log_simbus(
                        "    SignalValue: %u = <binary> (len=%u) [name=%s]",
                        sv->uid, sc->vector.length[v_idx], sv->name);
                } else {
                    double scalar_val = sc->vector.scalar[v_idx];
                    if (sv->val != scalar_val) {
                        log_simbus("    SignalValue: %u = %f [name=%s]",
                            sv->uid, scalar_val, sv->name);
                    }
                }
            }
        }
    }

    /* Pass 2: Execution hot-path. */
    for (uint32_t ch_idx = 0; ch_idx < ch_count; ch_idx++) {
        ChannelIndexItem* channel_item = VECTOR_AT(&am->channels, ch_idx);
        Channel*          ch = channel_item->ch;
        SimbusChannel*    sc =
            ch->mfc && !ch->is_binary
                   ? ((ModelFunctionChannel*)ch->mfc)->scalar_sync.simbus_channel
                   : NULL;
        if (sc == NULL) sc = _get_simbus_channel(v, ch->name);
        _refresh_index(ch);

        uint32_t index_count = ch->index.count;
        if (index_count == 0) continue;

        SignalMap* index_map = ch->index.map;
        if (ch->is_binary) {
            for (uint32_t i = 0; i < index_count; i++) {
                SignalValue* sv = index_map[i].signal;
                uint32_t     v_idx = sv->vector_index;

                if (sc->vector.length[v_idx]) {
                    dse_buffer_append(&sv->bin, &sv->bin_size,
                        &sv->bin_buffer_size, sc->vector.binary[v_idx],
                        sc->vector.length[v_idx]);
                }
            }
        } else {
            if (adapter->sequential_cosim) {
                for (uint32_t i = 0; i < index_count; i++) {
                    SignalValue* sv = index_map[i].signal;
                    sv->val = sv->final_val;
                }
            }
        }
    }

    return 0;
}


/**
adapter_loopb_model_start
=========================

SimBus -[NotifyMessage]-> Model

Starts the next model execution.

Parameters
----------
adapter (Adapter*)
: Pointer to the adapter object.

Returns
-------
0 (int)
: Function completed successfully.

rc (int)
: Non-zero value indicates failure.
 */
static int adapter_loopb_model_start(Adapter* adapter)
{
    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;

    if (v->state != ADAPTER_STATE_START) {
        v->state = ADAPTER_STATE_START;
    }

    /* Legacy method (binary and sequential co-sim).*/
    vector_foreach(&adapter->models, notify_update_sv, NULL);

    /* Flat-Map method (scalars). */
    flatmap_push_to_models(v, adapter);
    vector_foreach(&v->channels, flatmap_clear_tracking, NULL);

    return 0;
}


void adapter_loopb_destroy(Adapter* adapter)
{
    assert(adapter);
    if (adapter->vtable == NULL) return;

    AdapterLoopbVTable* v = (AdapterLoopbVTable*)adapter->vtable;
    vector_foreach(&v->channels, _destroy_channel, v);
    vector_reset(&v->channels);
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

    /* Set mode flags. */
    v->vtable.mode.flat_scalar = true;

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

    SimbusChannel* sc = *(SimbusChannel**)map_item;
    if (sc && sc->is_binary && sc->vector.count > 0) {
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
    vector_foreach(&v->channels, _binary_reset, NULL);
}
