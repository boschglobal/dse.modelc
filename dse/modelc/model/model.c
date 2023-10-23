// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/collections/set.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/model.h>
#include <dse/modelc/schema.h>


#define VECTOR_TYPE_BINARY_STR "binary"

typedef struct __signal_list_t {
    const char** names;
    uint32_t     length;
} __signal_list_t;


typedef enum ModelChannelType {
    MODEL_VECTOR_DOUBLE = 0, /* Double by default. */
    MODEL_VECTOR_BINARY
} ModelChannelType;


void model_function_destroy(ModelFunction* model_function)
{
    if (model_function) {
        char**   _keys = hashmap_keys(&model_function->channels);
        uint32_t _keys_length = hashmap_number_keys(model_function->channels);
        for (uint32_t i = 0; i < _keys_length; i++) {
            ModelFunctionChannel* _mfc =
                hashmap_get(&model_function->channels, _keys[i]);
            if (_mfc && _mfc->signal_value_double)
                free(_mfc->signal_value_double);
            if (_mfc && _mfc->signal_value_binary) {
                for (uint32_t _ = 0; _ < _mfc->signal_count; _++)
                    free((void*)_mfc->signal_value_binary[_]);
                free(_mfc->signal_value_binary);
            }
            if (_mfc && _mfc->signal_value_binary_size)
                free(_mfc->signal_value_binary_size);
            if (_mfc && _mfc->signal_value_binary_buffer_size)
                free(_mfc->signal_value_binary_buffer_size);
            if (_mfc && _mfc->signal_names) {
                // fixme Unit tests suggest a double free here.
                // for (uint32_t _ = 0; _ < _mfc->signal_count; _++)
                //     free((void*)_mfc->signal_names[_]);
                free(_mfc->signal_names);
            }
        }
        hashmap_destroy(&model_function->channels);
        for (uint32_t _ = 0; _ < _keys_length; _++)
            free(_keys[_]);
        free(_keys);
    }
    free(model_function);
}


/**
model_function_register
=======================

Register a Model Function. A Model may register one or more Model Functions
with repeated calls to this function.

Parameters
----------
model_instance (ModelInstanceSpec*)
: The Model Instance object (provided via the `model_setup()` function of the
  Model API).

name (const char*)
: The name of the Model Function.

step_size (double)
: The step size of the Model Function.

do_step_handler (ModelDoStepHandler)
: The "do step" function of the Model Function.

Returns
-------
0
: The model function was registered.

(errno)
: An error occurred during registration of the model function. The return
  value is the `errno` which may indicate the reason for the failure.

*/
int model_function_register(ModelInstanceSpec* model_instance, const char* name,
    double step_size, ModelDoStepHandler do_step_handler)
{
    int rc;
    errno = 0;

    /* Create the Model Function object. */
    ModelFunction* mf = calloc(1, sizeof(ModelFunction));
    if (mf == NULL) {
        log_error("ModelFunction malloc failed!");
        goto error_clean_up;
    }
    mf->name = name;
    mf->step_size = step_size;
    mf->do_step_handler = do_step_handler;
    rc = hashmap_init(&mf->channels);
    if (rc) {
        log_error("Hashmap init failed for channels!");
        if (errno == 0) errno = rc;
        goto error_clean_up;
    }
    /* Register the object with the Controller. */
    rc = controller_register_model_function(model_instance, mf);
    if (rc && (errno != EEXIST)) goto error_clean_up;
    return 0;

error_clean_up:
    model_function_destroy(mf);
    return errno;
}


static ModelFunctionChannel* _get_mfc(ModelInstanceSpec* model_instance,
    const char* model_function_name, const char* channel_name)
{
    ModelFunction* mf =
        controller_get_model_function(model_instance, model_function_name);
    if (mf == NULL) {
        log_error("ModelFunction not registered!");
        return NULL;
    }
    ModelFunctionChannel* mfc = hashmap_get(&mf->channels, channel_name);
    if (mfc) return mfc; /* Already allocated, return. */

    /* Allocate a new MFC. */
    errno = 0;
    mfc = calloc(1, sizeof(ModelFunctionChannel));
    if (mfc == NULL) {
        log_error("ModelFunction malloc failed!");
        return NULL;
    }
    mfc->channel_name = channel_name;
    hashmap_set_alt(&mf->channels, channel_name, mfc);

    return mfc;
}


static void _load_propagator_signal_names(
    SimpleSet* set, YamlNode* signals_node, bool target_channel)
{
    if (signals_node == NULL) return;

    uint32_t _sig_count = hashlist_length(&signals_node->sequence);
    for (uint32_t i = 0; i < _sig_count; i++) {
        YamlNode* sig_node = hashlist_at(&signals_node->sequence, i);
        YamlNode* n_node = NULL;
        if (target_channel) {
            n_node = dse_yaml_find_node(sig_node, "target");
        } else {
            n_node = dse_yaml_find_node(sig_node, "source");
        }
        /* Fallback the the common "signal" specifier. */
        if (n_node == NULL) {
            n_node = dse_yaml_find_node(sig_node, "signal");
        }
        set_add(set, n_node->scalar);
    }
}


static void _find_signals_node_legacy(ModelInstanceSpec* model_instance,
    const char* channel_name, YamlNode** channels_node, YamlNode** signals_node)
{
    *channels_node = NULL;
    *signals_node = NULL;
    const char* selectors[] = { "name" };
    const char* values[] = { channel_name };

    /* Search in the Model definition. */
    if (model_instance->model_definition.doc) {
        YamlNode* c_node =
            dse_yaml_find_node_in_seq(model_instance->model_definition.doc,
                "spec/channels", selectors, values, 1);
        YamlNode* s_node = dse_yaml_find_node(c_node, "signals");
        if (c_node && s_node) {
            log_info("Signals for channel[%s] selected from Model Definition.",
                channel_name);
            *channels_node = c_node;
            *signals_node = s_node;
            return;
        }
    }

    /* Search in the Model Instance. */
    if (model_instance->spec) {
        YamlNode* c_node = dse_yaml_find_node_in_seq(
            model_instance->spec, "channels", selectors, values, 1);
        YamlNode* s_node = dse_yaml_find_node(c_node, "signals");
        if (c_node && s_node) {
            log_info("Signals for channel[%s] selected from Model Instance.",
                channel_name);
            *channels_node = c_node;
            *signals_node = s_node;
            return;
        }
    }
}


static HashList         __handler_signal_list;
static ModelChannelType __handler_signal_vector_type;


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
        if (so->signal) {
            /* Signals are taken in parsing order. */
            hashlist_append(&__handler_signal_list, (void*)so->signal);
        }
        free(so);
    } while (1);

    /* Determine the signal vector type. */
    YamlNode* node;
    node = dse_yaml_find_node(object->doc, "metadata/annotations/vector_type");
    if (node && node->scalar) {
        if (strcmp(node->scalar, VECTOR_TYPE_BINARY_STR) == 0) {
            __handler_signal_vector_type = MODEL_VECTOR_BINARY;
        }
    }

    return 0;
}


ChannelSpec* model_build_channel_spec(
    ModelInstanceSpec* model_instance, const char* channel_name)
{
    log_debug("Search for channel on MI (%s) by name/alias=%s",
        model_instance->name, channel_name);
    const char* selectors[] = { "name" };
    const char* values[] = { channel_name };
    YamlNode*   c_node = dse_yaml_find_node_in_seq(
          model_instance->spec, "channels", selectors, values, 1);
    if (c_node) {
        log_debug(" channel found by name");
    } else {
        const char* selectors[] = { "alias" };
        const char* values[] = { channel_name };
        c_node = dse_yaml_find_node_in_seq(
            model_instance->spec, "channels", selectors, values, 1);
        if (c_node) {
            log_debug("  channel found by alias");
        }
    }
    if (c_node == NULL) {
        log_error("Channel node (%s) not found on MI (%s)!", channel_name,
            model_instance->name);
        return NULL;
    }

    ChannelSpec* channel_spec = calloc(1, sizeof(ChannelSpec));
    channel_spec->name = channel_name;
    channel_spec->private = c_node;
    YamlNode* n_node = dse_yaml_find_node(c_node, "name");
    if (n_node && n_node->scalar) channel_spec->name = n_node->scalar;
    YamlNode* a_node = dse_yaml_find_node(c_node, "alias");
    if (a_node && a_node->scalar) channel_spec->alias = a_node->scalar;

    return channel_spec; /* Caller to free. */
}


void _load_signals(ModelInstanceSpec* model_instance, ChannelSpec* channel_spec,
    __signal_list_t* signal_list, ModelChannelType* vector_type)
{
    /* Setup handler related storage. */
    hashlist_init(&__handler_signal_list, 512);
    __handler_signal_vector_type = *vector_type;
    /* Select and handle the schema objects (default name to provided name). */
    SchemaObjectSelector* selector = schema_build_channel_selector(
        model_instance, channel_spec, "SignalGroup");
    if (selector) {
        schema_object_search(
            model_instance, selector, _signal_group_match_handler);
    }
    /* Setup the final signal list. */
    signal_list->length = hashlist_length(&__handler_signal_list);
    if (signal_list->length) {
        signal_list->names = calloc(signal_list->length, sizeof(const char*));
        for (uint32_t i = 0; i < signal_list->length; i++) {
            signal_list->names[i] = hashlist_at(&__handler_signal_list, i);
            log_info("  signal[%u] : %s", i, signal_list->names[i]);
        }
    }
    *vector_type = __handler_signal_vector_type;
    /* Clear handler related storage. */
    schema_release_selector(selector);
    hashlist_destroy(&__handler_signal_list);
}


void _load_propagator_signals(ModelInstanceSpec* model_instance,
    ModelChannelDesc* channel_desc, __signal_list_t* signal_list)
{
    YamlNode* propagators_node = model_instance->propagators;
    /* Propagators are loaded dynamically from one or more YAML Docs
    which means duplicate signal name might exist. Therefore parse the
    signal names into a Set to avoid duplicates.

    NOTE: The order of signals for a propagator does not matter as the
    propagator model will dynamically load the related configuration direct
    from the YAML Documents. Therefore a Set is viable intermediate storage
    container.
    */
    SimpleSet signal_name_set;
    set_init(&signal_name_set);
    /* Parse out the signals from all listed propagators. */
    uint32_t _prop_count = hashlist_length(&propagators_node->sequence);
    for (uint32_t i = 0; i < _prop_count; i++) {
        YamlNode*   prop_node = hashlist_at(&propagators_node->sequence, i);
        YamlNode*   prop_name_node = dse_yaml_find_node(prop_node, "name");
        // Find the propagator.
        const char* selector[] = { "metadata/name" };
        const char* value[] = { prop_name_node->scalar };
        YamlNode*   p_doc = dse_yaml_find_doc_in_doclist(
              model_instance->yaml_doc_list, "Propagator", selector, value, 1);
        assert(p_doc);
        // Find the signals node.
        YamlNode* signals_node = dse_yaml_find_node(p_doc, "spec/signals");
        _load_propagator_signal_names(&signal_name_set, signals_node,
            channel_desc->propagator_target_channel);
    }
    /* Get the list of signals. */
    uint64_t _sig_count64 = 0;
    signal_list->names =
        (const char**)set_to_array(&signal_name_set, &_sig_count64);
    signal_list->length = _sig_count64;
    set_destroy(&signal_name_set);
}


void _load_signals_legacy(ModelInstanceSpec* model_instance,
    const char* channel_name, __signal_list_t* signal_list)
{
    /* Models are expected to define and reference signals according to the
    index of the signal in the Model Definition.

    IMPORTANT: signals must be parsed in the order which they occur in the
    Model Definition YAML Doc.
    */
    YamlNode* c_node = NULL;
    YamlNode* signals_node = NULL;
    _find_signals_node_legacy(
        model_instance, channel_name, &c_node, &signals_node);
    if (c_node == NULL) {
        log_fatal("Channel (%s) not found in model definition", channel_name);
    }
    if (signals_node == NULL) {
        log_fatal("Signals node not found in model definition ");
    }
    assert(c_node);
    assert(signals_node);
    signal_list->length = hashlist_length(&signals_node->sequence);
    signal_list->names = calloc(signal_list->length, sizeof(const char*));
    for (uint32_t i = 0; i < signal_list->length; i++) {
        YamlNode* sig_node = hashlist_at(&signals_node->sequence, i);
        YamlNode* n_node = dse_yaml_find_node(sig_node, "signal");
        signal_list->names[i] = n_node->scalar;
    }
}


/**
model_configure_channel
=======================

Configure a connection from this Model to a Channel on the Simulation Bus. The
Channel can then be represented by a Signal Vector making access to individual
Signals and their configuration (annotations) easy.

Parameters
----------
model_instance (ModelInstanceSpec*)
: The Model Instance object (provided via the `model_setup()` function of the
  Model API).

channel_desc (ModelChannelDesc*)
: A channel descriptor object which defines the Channel and Model Function names
  which should be configured.

Returns
-------
0
: The Channel was configured.

+VE
: An error occurred during the registration of the Channel.

*/
int model_configure_channel(
    ModelInstanceSpec* model_instance, ModelChannelDesc* channel_desc)
{
    assert(channel_desc);
    assert(model_instance);

    /* Determine the channel configuration. */
    log_notice("Configure Channel: %s", channel_desc->name);
    ChannelSpec* channel_spec =
        model_build_channel_spec(model_instance, channel_desc->name);
    if (channel_spec == NULL) return 1;
    log_notice("  Channel Name: %s", channel_spec->name);
    log_notice("  Channel Alias: %s", channel_spec->alias);

    /* Get an MFC object. */
    ModelFunctionChannel* mfc = _get_mfc(
        model_instance, channel_desc->function_name, channel_spec->name);
    if (mfc == NULL) {
        free(channel_spec);
        return 1;
    }

    /* Already configured? Then return.*/
    if (mfc->signal_names && mfc->signal_count) {
        // Enforce that only one signal vector is set, no matter what.
        if (mfc->signal_value_double)
            channel_desc->vector_double = mfc->signal_value_double;
        else if (mfc->signal_value_binary)
            channel_desc->vector_binary = mfc->signal_value_binary;
        else {
            log_error("Already configured channel did not have initialised "
                      "signal vector!");
            free(channel_spec);
            return 1;
        }
        log_notice(
            "Previously configured channel detected: %s", channel_spec->name);
        channel_desc->signal_count = mfc->signal_count;
        free(channel_spec);
        return 0;
    }

    /* Find the signal definitions:
    If the Model Instance specifies a Propagator, then the signals will be
    defined under Propagator:spec/signals.
    Otherwise, the signals will be under Model:spec/channels[name]/signals.
    */
    __signal_list_t  signal_list = { NULL, 0 };
    ModelChannelType vector_type = MODEL_VECTOR_DOUBLE;
    assert(model_instance->spec);
    if (model_instance->propagators) {
        _load_propagator_signals(model_instance, channel_desc, &signal_list);
    } else {
        /* Load signals via SignalGroups. */
        _load_signals(model_instance, channel_spec, &signal_list, &vector_type);
        if (signal_list.length == 0) {
            /* Fallback to legacy method. */
            _load_signals_legacy(
                model_instance, channel_spec->name, &signal_list);
        }
    }
    log_notice("  Unique signals identified: %u", signal_list.length);

    /* Init the channel and register signals. */
    controller_init_channel(model_instance, channel_spec->name,
        signal_list.names, signal_list.length);

    free(channel_spec);

    /* Allocate the Signal Vector and set the MFC and Channel_Desc members. */
    log_info("Allocate signal vector type %d for %u signals.", vector_type,
        signal_list.length);
    if (vector_type == MODEL_VECTOR_DOUBLE) {
        mfc->signal_value_double = calloc(signal_list.length, sizeof(double));
        channel_desc->vector_double = mfc->signal_value_double;
        log_debug("%p", channel_desc->vector_double);
    } else if (vector_type == MODEL_VECTOR_BINARY) {
        /* Allocate the binary vectors. */
        mfc->signal_value_binary = calloc(signal_list.length, sizeof(void*));
        mfc->signal_value_binary_size =
            calloc(signal_list.length, sizeof(uint32_t));
        mfc->signal_value_binary_buffer_size =
            calloc(signal_list.length, sizeof(uint32_t));
        /* Set the references to those vectors (in the Channel Desc).*/
        channel_desc->vector_binary = mfc->signal_value_binary;
        channel_desc->vector_binary_size = mfc->signal_value_binary_size;
        channel_desc->vector_binary_buffer_size =
            mfc->signal_value_binary_buffer_size;
    } else {
        log_error("Unsupported signal vector type! No signals available for "
                  "channel!");
        channel_desc->signal_count = 0;
        return 1;
    }

    /* MFC is owner and should free. */
    mfc->signal_count = channel_desc->signal_count = signal_list.length;
    mfc->signal_names = channel_desc->signal_names = signal_list.names;

    /* Brutal, eh? */
    return 0;
}
