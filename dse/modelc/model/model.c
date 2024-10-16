// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/collections/set.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/model.h>
#include <dse/modelc/schema.h>


#define VECTOR_TYPE_BINARY_STR "binary"
#define VARNAME_MAXLEN         250
#define EXPAND_VAR_MAXLEN      1023


typedef struct __signal_list_t {
    const char**     names;
    uint32_t         length;
    SignalTransform* transform;
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
                for (uint32_t _ = 0; _ < _mfc->signal_count; _++) {
                    if ((void*)_mfc->signal_value_binary[_])
                        free((void*)_mfc->signal_value_binary[_]);
                }
                free(_mfc->signal_value_binary);
            }
            if (_mfc && _mfc->signal_value_binary_size)
                free(_mfc->signal_value_binary_size);
            if (_mfc && _mfc->signal_value_binary_buffer_size)
                free(_mfc->signal_value_binary_buffer_size);
            if (_mfc && _mfc->signal_value_binary_reset_called)
                free(_mfc->signal_value_binary_reset_called);
            if (_mfc && _mfc->signal_names) {
                free(_mfc->signal_names);
            }
            if (_mfc && _mfc->signal_map) free(_mfc->signal_map);
            if (_mfc && _mfc->signal_transform) free(_mfc->signal_transform);
        }
        hashmap_destroy(&model_function->channels);
        for (uint32_t _ = 0; _ < _keys_length; _++)
            free(_keys[_]);
        free(_keys);
    }
    free(model_function);
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


static HashList         __handler_signal_list;
static ModelChannelType __handler_signal_vector_type;
static HashMap          __handler_transform_map;


static SignalTransform* _parse_signal_transform(SchemaSignalObject* so)
{
    YamlNode* linear_node = dse_yaml_find_node(so->data, "transform/linear");
    if (linear_node == NULL) return NULL;

    /* Create an object for the transform. */
    SignalTransform* st = malloc(sizeof(SignalTransform));
    *st = (SignalTransform){ .linear.factor = 1.0, .linear.offset = 0.0 };
    dse_yaml_get_double(linear_node, "factor", &st->linear.factor);
    dse_yaml_get_double(linear_node, "offset", &st->linear.offset);
    /* Linear factor *CANNOT* be 0 ... the transform is effectively disabled. */
    if (st->linear.factor == 0.0) {
        log_notice("Signal (%s): linear transform factor configured as 0 "
                   "(invalid value), transform disabled!",
            so->signal);
    }

    return st;
}


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
            /* Internal signals are skipped. */
            bool internal = false;
            dse_yaml_get_bool(so->data, "annotations/internal", &internal);
            if (internal) {
                free(so);
                continue;
            }

            /* Signals are taken in parsing order. */
            hashlist_append(&__handler_signal_list, (void*)so->signal);

            /* Locate an associated signal transform. */
            SignalTransform* st = _parse_signal_transform(so);
            if (st) hashmap_set_alt(&__handler_transform_map, so->signal, st);
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
    hashmap_init(&__handler_transform_map);
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
            SignalTransform* st =
                hashmap_get(&__handler_transform_map, signal_list->names[i]);
            if (st)
                log_info("    transform[linear] : factor=%f, offset=%f",
                    st->linear.factor, st->linear.offset);
        }
    }
    *vector_type = __handler_signal_vector_type;
    /* Construct the signal transform list. */
    if (hashmap_number_keys(__handler_transform_map)) {
        signal_list->transform =
            calloc(signal_list->length, sizeof(SignalTransform));
        for (size_t i = 0; i < signal_list->length; i++) {
            SignalTransform* st =
                hashmap_get(&__handler_transform_map, signal_list->names[i]);
            if (st == NULL) continue;
            /* Copy over the transform object. */
            memcpy(&signal_list->transform[i], st, sizeof(SignalTransform));
        }
    }
    /* Clear handler related storage. */
    schema_release_selector(selector);
    hashlist_destroy(&__handler_signal_list);
    hashmap_destroy(&__handler_transform_map);
}


/*
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

name (const char*)
: Channel name (or alias).

function_name (const char*)
: Model Function name.

Returns
-------
0
: The Channel was configured.

+VE
: An error occurred during the registration of the Channel.

*/
int model_configure_channel(ModelInstanceSpec* model_instance, const char* name,
    const char* function_name)
{
    assert(model_instance);

    /* Determine the channel configuration. */
    log_notice("Configure Channel: %s", name);
    ChannelSpec* channel_spec = model_build_channel_spec(model_instance, name);
    if (channel_spec == NULL) return 1;
    log_notice("  Channel Name: %s", channel_spec->name);
    log_notice("  Channel Alias: %s", channel_spec->alias);

    /* Get an MFC object. */
    ModelFunctionChannel* mfc =
        _get_mfc(model_instance, function_name, channel_spec->name);
    if (mfc == NULL) {
        free(channel_spec);
        return 1;
    }

    /* Already configured? Then return.*/
    if (mfc->signal_names && mfc->signal_count) {
        // Enforce that only one signal vector is set, no matter what.
        if (mfc->signal_value_double == NULL &&
            mfc->signal_value_binary == NULL) {
            log_error("Already configured channel did not have initialised "
                      "signal vector!");
            free(channel_spec);
            return 1;
        }
        log_notice(
            "Previously configured channel detected: %s", channel_spec->name);
        free(channel_spec);
        return 0;
    }

    /* Load signals via SignalGroups. */
    __signal_list_t  signal_list = { NULL, 0 };
    ModelChannelType vector_type = MODEL_VECTOR_DOUBLE;
    assert(model_instance->spec);
    _load_signals(model_instance, channel_spec, &signal_list, &vector_type);
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
        log_debug("%p", mfc->signal_value_double);
    } else if (vector_type == MODEL_VECTOR_BINARY) {
        /* Allocate the binary vectors. */
        mfc->signal_value_binary = calloc(signal_list.length, sizeof(void*));
        mfc->signal_value_binary_size =
            calloc(signal_list.length, sizeof(uint32_t));
        mfc->signal_value_binary_buffer_size =
            calloc(signal_list.length, sizeof(uint32_t));
        mfc->signal_value_binary_reset_called =
            calloc(signal_list.length, sizeof(bool));
    } else {
        log_error("Unsupported signal vector type! No signals available for "
                  "channel!");
        return 1;
    }

    /* MFC is owner and should free. */
    mfc->signal_count = signal_list.length;
    mfc->signal_names = signal_list.names;
    mfc->signal_transform = signal_list.transform;

    /* Brutal, eh? */
    return 0;
}


static void _index_signal_vector(SignalVector* sv)
{
    /* Reset the index object. */
    if (sv->index) {
        hashmap_destroy(sv->index);
        free(sv->index);
        sv->index = NULL;
    }
    sv->index = calloc(1, sizeof(HashMap));
    hashmap_init(sv->index);
    /* Build the index. */
    for (uint32_t i = 0; i < sv->count; i++) {
        hashmap_set_long(sv->index, sv->signal[i], (int32_t)i);
    }
}

DLL_PRIVATE ModelSignalIndex __model_index__(
    ModelDesc* m, const char* vname, const char* sname)
{
    ModelSignalIndex index = {};
    if (m == NULL || m->sv == NULL || vname == NULL) return index;

    SignalVector* sv = m->sv;
    uint32_t      v_idx = 0;
    while (sv && sv->name) {
        log_trace("Index search (vector) %s - %s", sv->alias, vname);
        if (strcmp(sv->alias, vname) == 0) {
            /* Vector match only? */
            if (sname == NULL) {
                index.sv = sv;
                index.vector = v_idx;
                return index;
            }
            /* Signal match. */
            if (sv->index == NULL) _index_signal_vector(sv);
            log_trace("Index search (signal) %s", sname);
            uint32_t* s_idx = hashmap_get(sv->index, sname);
            if (s_idx) {
                /* Match! */
                index.sv = sv;
                index.vector = v_idx;
                index.signal = *s_idx;
                if (sv->is_binary) {
                    index.binary = &(sv->binary[*s_idx]);
                } else {
                    index.scalar = &(sv->scalar[*s_idx]);
                }
                return index;
            }
        }
        sv++;
        v_idx++;
    }
    return index;
}


/**
model_create
============

> Optional method of `ModelVTable` interface.

Called by the Model Runtime to create a new instance of this model.

The `model_create()` method may extend or mutilate the provided Model
Descriptor. When extending the Model Descriptor _and_ allocating additional
resources then the `model_destroy()` method should also be implemented.

Fault conditions can be communicated to the caller by setting variable
`errno` to a non-zero value. Additionally, `log_fatal()` can be used to
immediately halt execution of a model.

Parameters
----------
model (ModelDesc*)
: The Model Descriptor object representing an instance of this model.

Returns
-------
NULL
: The Channel was configured.

(ModelDesc*)
: Pointer to a new, or mutilated, version of the Model Descriptor object. The
  original Model Descriptor object will be released by the Model Runtime (i.e.
  don't call `free()`).

errno <> 0 (indirect)
: Indicates an error condition.

Example
-------

{{< readfile file="../examples/model_create.c" code="true" lang="c" >}}


*/
extern ModelDesc* model_create(ModelDesc* m);


/**
model_step
==========

> Mandatory method of `ModelVTable` interface. Alternatively, Model implementers
  may specify the `ModelVTable.step` method dynamically by mutilating the
  Model Descriptor in the `model_create()` method, or even at runtime.

Called by the Model Runtime to step the model for a time interval.

Parameters
----------
model (ModelDesc*)
: The Model Descriptor object representing an instance of this model.

model_time (double*)
: (in/out) Specifies the model time for this step of the model.

stop_time (double)
: Specifies the stop time for this step of the model. The model step should not
  exceed this time.

Returns
-------
0
: The step completed without error.

<>0
: An error occurred at some point during the step execution.

model_time (via parameter)
: The final model time reached for this step. This value may be less than
  `stop_time` if a step decides to return early.
*/
extern int model_step(ModelDesc* model, double* model_time, double stop_time);


/**
model_destroy
=============

> Optional method of `ModelVTable` interface.

Called by the Model Runtime at the end of a simulation, the `model_destroy()`
function may be implemented by a Model Integrator to perform any custom
cleanup operations (e.g. releasing instance related resources, such as open
files or allocated memory).

Parameters
----------
model (ModelDesc*)
: The Model Descriptor object representing an instance of this model.

*/
extern void model_destroy(ModelDesc* model);


/**
model_index_
============

> Provided method (by the Runtime). Model implementers may specify
  a different index method by mutilating the Model Descriptor in the
  `model_create()` method, or even at runtime.

A model may use this method to index a signal that is contained within the
Signal Vectors of the Model Descriptor.

Parameters
----------
model (ModelDesc*)
: The Model Descriptor object representing an instance of this model.

vname (const char*)
: The name (alias) of the Signal Vector.

sname (const char*)
: The name of the signal within the Signal Vector. When set to NULL the index
  will match on Signal Vector (vanme) only.

Returns
-------
ModelSignalIndex
: An index. When valid, either the `scalar` or `binary` fields will be set to
  a valid pointer (i.e. not NULL). When `sname` is not specified the index will
  contain a valid pointer to a Signal Vector object only (i.e. both `scalar`
  and `binary` will be set to NULL).

*/
extern ModelSignalIndex model_index_(
    ModelDesc* model, const char* vname, const char* sname);


/**
model_annotation
================

Retrieve the specified annotation from the Model specification.

Parameters
----------
model (ModelDesc*)
: The Model Descriptor object representing an instance of this model.

name (const char*)
: The name of the annotation.

Returns
-------
const char*
: The value of the specified annotation.

NULL
: The specified annotation was not found.

*/
const char* model_annotation(ModelDesc* model, const char* name)
{
    if (model && model->mi) {
        YamlNode* a = dse_yaml_find_node(
            model->mi->model_definition.doc, "metadata/annotations");
        if (a) return dse_yaml_get_scalar(a, name);
    }
    return NULL;
}


/**
model_instance_annotation
=========================

Retrieve the specified annotation from the Model instance (Stack specification).

Parameters
----------
model (ModelDesc*)
: The Model Descriptor object representing an instance of this model.

name (const char*)
: The name of the annotation.

Returns
-------
const char*
: The value of the specified annotation.

NULL
: The specified annotation was not found.

*/
const char* model_instance_annotation(ModelDesc* model, const char* name)
{
    if (model && model->mi) {
        YamlNode* a = dse_yaml_find_node(model->mi->spec, "annotations");
        if (a) return dse_yaml_get_scalar(a, name);
    }
    return NULL;
}


/**
model_expand_vars
=================

Expand environment variables in a string according to typical shell
variable expansion (i.e ${FOO} or ${BAR:-default}). Environment variables
are searched first with the Model Instance Name prefixed to the variable
name with a "__" delimiter (i.e. "INSTANCE_NAME__ENVAR"), and then if no
match is found, the search is repated with only the provied variable name.

> Note: Variables are converted to upper case before searching the environment.

Parameters
----------
model (ModelDesc*)
: The Model Descriptor object representing an instance of this model.

source (const char*)
: The string containing environment variables to expand.

Returns
-------
char*
: String with environment variables expanded. Caller to free.
*/
char* model_expand_vars(ModelDesc* m, const char* source)
{
    char* source_copy = strdup(source);
    char* haystack = source_copy;
    char* result = calloc(EXPAND_VAR_MAXLEN + 1, sizeof(char));

    while (haystack) {
        /* Search for START. */
        char* _var = strstr(haystack, "${");
        if (_var == NULL) {
            strncat(result, haystack, EXPAND_VAR_MAXLEN - strlen(result));
            break;
        }
        /* Copy any preceding chars to the result. */
        *_var = '\0';
        strncat(result, haystack, EXPAND_VAR_MAXLEN - strlen(result));
        /* Search for END. */
        _var += 2;
        char* _var_end = strstr(_var, "}");
        if (_var_end == NULL) {
            /* Did not find the end, GIGO. */
            strncat(result, haystack, EXPAND_VAR_MAXLEN - strlen(result));
            break;
        }
        *_var_end = '\0';
        haystack = _var_end + 1; /* Setup for next iteration. */

        /* Does the VAR have a DEFAULT? */
        char* _def = strstr(_var, ":-");
        if (_def) {
            *_def = '\0';
            _def += 2;
        }
        /* Do the lookup. */
        char* _env_val = NULL;
        char  _v[VARNAME_MAXLEN] = {};
        if (m && m->mi && m->mi->name) {
            /* Search with Model Instance Name as prefix to VAR. */
            snprintf(_v, VARNAME_MAXLEN, "%s__%s", m->mi->name, _var);
            for (char* p = _v; *p; p++) {
                *p = toupper(*p);
            }
            _env_val = getenv(_v);
        } else {
            /* Bad ModeDesc object, disable the first search. */
            _env_val = NULL;
        }
        if (_env_val) {
            /* Match on VAR with prefix. */
            strncat(result, _env_val, EXPAND_VAR_MAXLEN - strlen(result));
        } else {
            /* Search with VAR only. */
            snprintf(_v, VARNAME_MAXLEN, "%s", _var);
            for (char* p = _v; *p; p++) {
                *p = toupper(*p);
            }
            _env_val = getenv(_v);
            if (_env_val) {
                /* Match on VAR. */
                strncat(result, _env_val, EXPAND_VAR_MAXLEN - strlen(result));
            } else if (_def) {
                /* No match, use default. */
                strncat(result, _def, EXPAND_VAR_MAXLEN - strlen(result));
            } else {
                /* No match, no default, GIGO. Use original var/case. */
                strncat(result, _var, EXPAND_VAR_MAXLEN - strlen(result));
            }
        }
    }
    free(source_copy);
    return result; /* Caller to free. */
}
