// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <assert.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/model.h>
#include <dse/modelc/schema.h>
#include <dse/modelc/controller/model_private.h>


#define UNUSED(x)                ((void)x)
#define DEFAULT_BINARY_MIME_TYPE "application/octet-stream"


/*
 *  Signal Annotation Functions
 *  ===========================
 */

static SchemaSignalObject* __signal_match;
static const char*         __signal_match_name;

static int _signal_group_match_handler(
    ModelInstanceSpec* model_instance, SchemaObject* object)
{
    uint32_t index = 0;

    /* Enumerate over the signals. */
    SchemaSignalObject* so;
    do {
        so = schema_object_enumerator(model_instance, object, "spec/signals",
            &index, schema_signal_object_generator);
        if (so == NULL) break;
        if (strcmp(so->signal, __signal_match_name) == 0) {
            __signal_match = so; /* Caller to free. */
            return 0;
        }
        free(so);
    } while (1);

    return 0;
}


static const char* _signal_annotation(ModelInstanceSpec* mi, SignalVector* sv,
    const char* signal, const char* name)
{
    const char* value = NULL;

    /* Set the search vars. */
    __signal_match = NULL;
    __signal_match_name = signal;

    /* Select and handle the schema objects. */
    ChannelSpec*          cs = model_build_channel_spec(mi, sv->name);
    SchemaObjectSelector* selector;
    selector = schema_build_channel_selector(mi, cs, "SignalGroup");
    if (selector) {
        schema_object_search(mi, selector, _signal_group_match_handler);
    }
    schema_release_selector(selector);
    free(cs);

    /* Look for the annotation. */
    if (__signal_match) {
        YamlNode* n = dse_yaml_find_node(__signal_match->data, "annotations");
        dse_yaml_get_string(n, name, &value);
        free(__signal_match);
        __signal_match = NULL;
    }

    return value;
}


/**
 *  Helper Functions
 *  ================
 */

static int __binary_append_nop(
    SignalVector* sv, uint32_t index, void* data, uint32_t len)
{
    UNUSED(sv);
    UNUSED(index);
    UNUSED(data);
    UNUSED(len);

    return 0;
}

static int __binary_reset_nop(SignalVector* sv, uint32_t index)
{
    UNUSED(sv);
    UNUSED(index);

    return 0;
}

static int __binary_release_nop(SignalVector* sv, uint32_t index)
{
    UNUSED(sv);
    UNUSED(index);

    return 0;
}

static int __binary_append(
    SignalVector* sv, uint32_t index, void* data, uint32_t len)
{
    dse_buffer_append(&sv->binary[index], &sv->length[index],
        &sv->buffer_size[index], data, len);

    return 0;
}

static int __binary_reset(SignalVector* sv, uint32_t index)
{
    sv->length[index] = 0;

    return 0;
}

static int __binary_release(SignalVector* sv, uint32_t index)
{
    free(sv->binary[index]);
    sv->binary[index] = NULL;
    sv->length[index] = 0;
    sv->buffer_size[index] = 0;

    return 0;
}

static const char* __annotation_get(
    SignalVector* sv, uint32_t index, const char* name)
{
    assert(sv);
    assert(sv->mi);

    return _signal_annotation(sv->mi, sv, sv->signal[index], name);
}


/*
 *  Enumerator Functions
 *  ====================
 *
 *  For model_sv_create().
 */

typedef struct sv_data {
    ModelInstanceSpec* mi;
    SignalVector*      collection;
    uint32_t           collection_max_index;
    uint32_t           current_collection_index;
    const char*        current_modelfunction_name;
} sv_data;

static int _add_sv(void* _mfc, void* _sv_data)
{
    ModelFunctionChannel* mfc = _mfc;
    sv_data*              data = _sv_data;

    if (data->collection_max_index <= data->current_collection_index) {
        log_error("New Index %d exceeds max index %d",
            data->current_collection_index, data->collection_max_index);
        return -1;
    }
    SignalVector* current_sv =
        &(data->collection[data->current_collection_index]);

    current_sv->name = mfc->channel_name;
    current_sv->count = mfc->signal_count;
    current_sv->signal = mfc->signal_names;
    current_sv->function_name = data->current_modelfunction_name;
    current_sv->annotation = __annotation_get;
    current_sv->mi = data->mi;
    if (mfc->signal_value_binary) {
        current_sv->is_binary = true;
        current_sv->binary = mfc->signal_value_binary;
        current_sv->length = mfc->signal_value_binary_size;
        current_sv->buffer_size = mfc->signal_value_binary_buffer_size;
        current_sv->append = __binary_append;
        current_sv->reset = __binary_reset;
        current_sv->release = __binary_release;
        /* Mime Type. */
        current_sv->mime_type = calloc(current_sv->count, sizeof(char*));
        for (uint32_t i = 0; i < current_sv->count; i++) {
            current_sv->mime_type[i] = DEFAULT_BINARY_MIME_TYPE;
            const char* mt;
            mt = current_sv->annotation(current_sv, i, "mime_type");
            if (mt) current_sv->mime_type[i] = mt;
        }
    } else {
        current_sv->is_binary = false;
        current_sv->scalar = mfc->signal_value_double;
        current_sv->append = __binary_append_nop;
        current_sv->reset = __binary_reset_nop;
        current_sv->release = __binary_release_nop;
    }

    /* Progress the data object to the next item (for next call). */
    data->current_collection_index++;
    return 0;
}

static int _collect_sv(void* _mf, void* _sv_data)
{
    ModelFunction* mf = _mf;
    sv_data*       data = _sv_data;

    data->current_modelfunction_name = mf->name;

    return (hashmap_iterator(&mf->channels, _add_sv, false, data));
}

static int _count_sv(void* _mf, void* _number)
{
    ModelFunction* mf = _mf;
    uint64_t*      number = _number;

    *number += hashmap_number_keys(mf->channels);

    return 0;
}


/**
 *  model_sv_create
 *
 *  This is Model User API replacing modelc_debug.c::modelc_get_model_vectors().
 *
 *  Parameters
 *  ----------
 *  mi : ModelInstanceSpec (pointer to)
 *      The model instance, which holds references to the registered channels.
 *
 *  Returns
 *  -------
 *      SignalVector (pointer to NULL terminated list) : A list of SignalVector
 *          objects representing the signals assigned to a channel. The list
 *          is NULL terminated (sv->name == NULL). Caller to free.
 *
 *  Example
 *  -------
 *  #include <dse/modelc/model.h>
 *  #include <dse/logger.h>
 *
 *  void print_signal_vectors(ModelInstanceSpec* mi)
 *  {
 *      SignalVector* sv = model_sv_create(mi);
 *      while (sv && sv->name) {
 *          log_debug("Signal Vector : %s", sv->name);
 *
 *          for (uint32_t i = 0; i < sv->count; i++) {
 *              log_debug("  signal : %s", sv->signal[i]);
 *              if (sv->is_binary) {
 *                  log_debug("    length : %s", sv->length[i]);
 *                  log_debug("    buffer_size : %s", sv->buffer_size[i]);
 *                  log_debug("    mime_type : %s", sv->mime_type[i]);
 *
 *                  // Example use of object functions.
 *                  sv->reset(sv, i);
 *                  sv->append(sv, i, "foo", 3);
 *                  sv->release(sv, i);
 *                  const char* mime_type = sv->annotation(sv, i, "mime_type");
 *                  if (mime_type) log_debug("    annotation : %s", mime_type);
 *              } else {
 *                  log_debug("  scalar : %s", sv->scalar[i]);
 *              }
 *          }
 *
 *          // Next signal vector.
 *          sv++;
 *      }
 *  }
 */
SignalVector* model_sv_create(ModelInstanceSpec* mi)
{
    int      rc;
    uint32_t count = 0;

    /* Count the signal vectors (e.g. channels). */
    ModelInstancePrivate* mip = mi->private;
    HashMap* model_function_map = &mip->controller_model->model_functions;
    rc = hashmap_iterator(model_function_map, _count_sv, false, &count);
    if (rc) {
        log_error("Iterating data structure failed");
        return NULL;
    }
    log_debug("Found %d signal vectors in model instance", count);

    /* Allocate and collect the signal vectors. */
    SignalVector* sv = calloc(count + 1, sizeof(SignalVector));
    sv_data       _sv_data = { mi, sv, count, 0, NULL };
    rc = hashmap_iterator(model_function_map, _collect_sv, false, &_sv_data);
    if (rc) {
        log_error("Iterating data structure failed");
        return NULL;
    }

    /* Return the signal vector list (NULL terminated). */
    return sv;
}


/**
 *  model_sv_destroy
 *
 *  The underlying objects of a SignalVector object (e.g. from ModelC object)
 *  are not affected by calling this method.
 *
 *  Parameters
 *  ----------
 *  sv : SignalVector (pointer to NULL terminated list)
 *      List of SignalVector objects to destroy.
 *
 *  Returns
 *  -------
 */
void model_sv_destroy(SignalVector* sv)
{
    SignalVector* sv_save = sv;

    while (sv && sv->name) {
        if (sv->mime_type) free(sv->mime_type);
        /* Next signal vector. */
        sv++;
    }

    free(sv_save);
}
