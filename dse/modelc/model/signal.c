// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/strings.h>
#include <dse/clib/util/yaml.h>
#include <dse/ncodec/codec.h>
#include <dse/modelc/runtime.h>
#include <dse/modelc/schema.h>
#include <dse/modelc/controller/model_private.h>


#define UNUSED(x)                ((void)x)
#define DEFAULT_BINARY_MIME_TYPE "application/octet-stream"


extern void ncodec_trace_configure(NCODEC* nc, ModelInstanceSpec* mi);
extern void ncodec_trace_destroy(NCodecInstance* nc);


__attribute__((unused)) static void __compile_time_checks(void)
{
/* Compile-time type size check. Get actual size with:
 *    char (*___)[sizeof(SignalVector)] = 1;
 */
#if __SIZEOF_POINTER__ == 8
    _Static_assert(sizeof(SignalVector) == 256, "Compatibility FAIL!");
#else
    _Static_assert(sizeof(SignalVector) == 168, "Compatibility FAIL!");
#endif
}


/* Signal Annotation Functions. */

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
        value = dse_yaml_get_scalar(n, name);
        free(__signal_match);
        __signal_match = NULL;
    }

    return value;
}


static const char* __signal_group_annotation_name;
static const char* __signal_group_annotation_value;

static int _sg_annotation_search_match_handler(
    ModelInstanceSpec* model_instance, SchemaObject* object)
{
    UNUSED(model_instance);

    YamlNode*   n = dse_yaml_find_node(object->doc, "metadata/annotations");
    const char* value = dse_yaml_get_scalar(n, __signal_group_annotation_name);
    if (value) {
        /* Match found, return +ve to stop search. */
        __signal_group_annotation_value = value;
        return 1;
    }
    return 0; /* Continue search. */
}

static const char* _signal_group_annotation(
    ModelInstanceSpec* mi, SignalVector* sv, const char* name)
{
    /* Set the search vars. */
    __signal_group_annotation_name = name;
    __signal_group_annotation_value = NULL;

    /* Search over the schema objects. */
    ChannelSpec*          cs = model_build_channel_spec(mi, sv->name);
    SchemaObjectSelector* selector;
    selector = schema_build_channel_selector(mi, cs, "SignalGroup");
    if (selector) {
        schema_object_search(mi, selector, _sg_annotation_search_match_handler);
    }
    schema_release_selector(selector);
    free(cs);

    /* If the search was successful (first match wins), the value will be set.
     */
    return __signal_group_annotation_value;
}


/* Helper Functions. */

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

static void* __binary_codec_nop(SignalVector* sv, uint32_t index)
{
    UNUSED(sv);
    UNUSED(index);

    return 0;
}

static int __binary_append(
    SignalVector* sv, uint32_t index, void* data, uint32_t len)
{
    if (sv->reset_called[index] == false) {
        errno = EPROTO;
        log_error("Binary Check: Model did not call Reset before Append!");
    }
    dse_buffer_append(&sv->binary[index], &sv->length[index],
        &sv->buffer_size[index], data, len);

    return 0;
}

static int __binary_reset(SignalVector* sv, uint32_t index)
{
    /* Note, changes here must be duplicated in stream_seek(). */
    sv->length[index] = 0;
    sv->reset_called[index] = true;

    NCodecInstance* nc = sv->ncodec[index];
    ncodec_seek((NCODEC*)nc, 0, NCODEC_SEEK_RESET);

    return 0;
}

static int __binary_release(SignalVector* sv, uint32_t index)
{
    __binary_reset(sv, index);

    free(sv->binary[index]);
    sv->binary[index] = NULL;
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

static const char* __group_annotation_get(SignalVector* sv, const char* name)
{
    assert(sv);
    assert(sv->mi);

    return _signal_group_annotation(sv->mi, sv, name);
}

static void* __binary_codec(SignalVector* sv, uint32_t index)
{
    assert(sv);
    assert(sv->mi);
    assert(index < sv->count);

    return sv->ncodec[index];
}


/* Enumerator Functions, for model_sv_create(). */

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
    current_sv->vtable.annotation = __annotation_get;
    current_sv->vtable.group_annotation = __group_annotation_get;
    current_sv->mi = data->mi;
    if (mfc->signal_value_binary) {
        current_sv->is_binary = true;
        current_sv->binary = mfc->signal_value_binary;
        current_sv->length = mfc->signal_value_binary_size;
        current_sv->buffer_size = mfc->signal_value_binary_buffer_size;
        current_sv->reset_called = mfc->signal_value_binary_reset_called;
        current_sv->vtable.append = __binary_append;
        current_sv->vtable.reset = __binary_reset;
        current_sv->vtable.release = __binary_release;
        current_sv->vtable.codec = __binary_codec;
        /* Mime Type. */
        current_sv->mime_type = calloc(current_sv->count, sizeof(char*));
        for (uint32_t i = 0; i < current_sv->count; i++) {
            current_sv->mime_type[i] = DEFAULT_BINARY_MIME_TYPE;
            const char* mt;
            mt = current_sv->vtable.annotation(current_sv, i, "mime_type");
            if (mt) current_sv->mime_type[i] = mt;
        }
        /* NCodec. */
        current_sv->ncodec = calloc(current_sv->count, sizeof(NCODEC*));
        for (uint32_t i = 0; i < current_sv->count; i++) {
            void*   stream = model_sv_stream_create(current_sv, i);
            NCODEC* nc = ncodec_open(current_sv->mime_type[i], stream);
            if (nc) {
                ncodec_trace_configure(nc, data->mi);
                current_sv->ncodec[i] = nc;
            } else {
                model_sv_stream_destroy(stream);
            }
        }
    } else {
        current_sv->is_binary = false;
        current_sv->scalar = mfc->signal_value_double;
        current_sv->vtable.append = __binary_append_nop;
        current_sv->vtable.reset = __binary_reset_nop;
        current_sv->vtable.release = __binary_release_nop;
        current_sv->vtable.codec = __binary_codec_nop;
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


/*
model_sv_create
===============

This is Model User API replacing modelc_debug.c::modelc_get_model_vectors().

Parameters
----------
mi (ModelInstanceSpec*)
: The model instance, which holds references to the registered channels.

Returns
-------
SignalVector (pointer to NULL terminated list)
: A list of SignalVector objects representing the signals assigned to a model.
  The list is NULL terminated (sv->name == NULL). Caller to free.

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

    /* Lookup channel aliases on the Model Instance. */
    SignalVector* sv_p = sv;
    while (sv_p && sv_p->name) {
        const char* selector[] = { "name" };
        const char* value[] = { sv_p->name };
        YamlNode*   ch_node =
            dse_yaml_find_node_in_seq(mi->spec, "channels", selector, value, 1);
        if (ch_node) {
            sv_p->alias = dse_yaml_get_scalar(ch_node, "alias");
        }
        /* Next signal vector. */
        sv_p++;
    }

    /* Return the signal vector list (NULL terminated). */
    return sv;
}


/*
model_sv_destroy
================

The underlying objects of a SignalVector object (e.g. from ModelC object)
are not affected by calling this method.

Parameters
----------
sv (SignalVector*)
: The SignalVector object to destroy. Should be the same object as returned
  from the call to `model_sv_create()`.

*/
void model_sv_destroy(SignalVector* sv)
{
    SignalVector* sv_save = sv;

    while (sv && sv->name) {
        if (sv->mime_type) free(sv->mime_type);
        if (sv->is_binary) {
            /* NCodec. */
            for (uint32_t i = 0; i < sv->count; i++) {
                NCodecInstance* nc = sv->ncodec[i];
                if (nc) {
                    ncodec_trace_destroy(nc);
                    model_sv_stream_destroy(nc->stream);
                    ncodec_close((NCODEC*)nc);
                    sv->ncodec[i] = NULL;
                }
            }
            free(sv->ncodec);
        }
        if (sv->index) {
            hashmap_destroy(sv->index);
            free(sv->index);
            sv->index = NULL;
        }

        /* Next signal vector. */
        sv++;
    }

    free(sv_save);
}


/**
signal_index
============

A model may use this method to index a signal that is contained within the
Signal Vectors of the Model Descriptor.

Parameters
----------
model (ModelDesc*)
: The Model Descriptor object representing an instance of this model.

vname (const char*)
: The name (alias) of the Signal Vector.

name (const char*)
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
ModelSignalIndex signal_index(
    ModelDesc* model, const char* vname, const char* name)
{
    if (model && model->index) {
        return model->index(model, vname, name);
    }
    return (ModelSignalIndex){};
}


/**
signal_read
===========

Read data from the specified binary signal. Returns pointer to the internal
binary signal buffers.

Parameters
----------
sv (SignalVector*)
: The Signal Vector object containing the signal.

index (uint32_t)
: Index of the signal in the Signal Vector object.

data (uint8_t*)
: Address/pointer to the data of the binary signal.

len (size_t)
: Length of the data.

Returns
-------
0
: The operation completed without error.

-EINVAL (-22)
: Bad arguments.

<>0
: Indicates an error condition. Inspect `errno` for additional information.
*/
inline int signal_read(
    SignalVector* sv, uint32_t index, uint8_t** data, size_t* len)
{
    if (data == NULL || len == NULL) return -EINVAL;
    *data = NULL;
    *len = 0;

    if (sv && index < sv->count && sv->is_binary) {
        *data = (uint8_t*)sv->binary[index];
        *len = (size_t)sv->length[index];
        return 0;
    } else {
        return -EINVAL;
    }
}


/**
signal_append
=============

Append data to the end of the specified binary signal. The append method will
resize the buffers of the binary signal as required.

Parameters
----------
sv (SignalVector*)
: The Signal Vector object containing the signal.

index (uint32_t)
: Index of the signal in the Signal Vector object.

data (uint8_t*)
: Address/pointer to the data which should be appended to the binary signal.

len (size_t)
: Length of the provided data buffer being appended.

Returns
-------
0
: The operation completed without error.

-EINVAL (-22)
: Bad arguments.

-ENOSYS (-88)
: The called function is not available.

<>0
: Indicates an error condition. Inspect `errno` for additional information.
*/
inline int signal_append(
    SignalVector* sv, uint32_t index, uint8_t* data, size_t len)
{
    if (sv && index < sv->count && sv->is_binary) {
        if (sv->vtable.append) {
            return sv->vtable.append(sv, index, (void*)data, len);
        } else {
            return -ENOSYS;
        }
    } else {
        return -EINVAL;
    }
}


/**
signal_reset
============

Reset a binary signal (e.g. sets its buffer length to 0). The buffers of the
binary signal are not released (see `signal_release()`).

Parameters
----------
sv (SignalVector*)
: The Signal Vector object containing the signal.

index (uint32_t)
: Index of the signal in the Signal Vector object.

Returns
-------
0
: The operation completed without error.

-EINVAL (-22)
: Bad arguments.

-ENOSYS (-88)
: The called function is not available.

<>0
: Indicates an error condition. Inspect `errno` for additional information.
*/
inline int signal_reset(SignalVector* sv, uint32_t index)
{
    if (sv && index < sv->count && sv->is_binary) {
        if (sv->vtable.reset) {
            return sv->vtable.reset(sv, index);
        } else {
            return -ENOSYS;
        }
    } else {
        return -EINVAL;
    }
}


/**
signal_release
==============

Release the resources allocated to a binary signal (e.g. free the buffer).

Parameters
----------
sv (SignalVector*)
: The Signal Vector object containing the signal.

index (uint32_t)
: Index of the signal in the Signal Vector object.

Returns
-------
0
: The operation completed without error.

-EINVAL (-22)
: Bad arguments.

-ENOSYS (-88)
: The called function is not available.

<>0
: Indicates an error condition. Inspect `errno` for additional information.
*/
inline int signal_release(SignalVector* sv, uint32_t index)
{
    if (sv && index < sv->count && sv->is_binary) {
        if (sv->vtable.release) {
            return sv->vtable.release(sv, index);
        } else {
            return -ENOSYS;
        }
    } else {
        return -EINVAL;
    }
}


/**
signal_codec
============

Return a pointer to the Codec object associated with a binary signal.

Codec objects are created when a binary signal is specified with a `mime_type`
annotation.

Parameters
----------
sv (SignalVector*)
: The Signal Vector object containing the signal.

index (uint32_t)
: Index of the signal in the Signal Vector object.

Returns
-------
void*
: The Codec object associated with the binary signal.

NULL
: The binary signal does not have an associated Codec object, inspect `errno`
for additional information..

Example (Codec Specification)
-------

```yaml
kind: SignalGroup
metadata:
  name: network
  labels:
    channel: network_vector
  annotations:
    vector_type: binary
spec:
  signals:
    - signal: can_bus
      annotations:
        mime_type: application/x-automotive-bus; interface=stream; type=frame;
bus=can; schema=fbs; bus_id=1; node_id=2; interface_id=3
```

Reference
---------

[Network Codec
API](https://github.com/boschglobal/dse.standards/tree/main/dse/ncodec)

*/
inline void* signal_codec(SignalVector* sv, uint32_t index)
{
    if (sv && index < sv->count && sv->is_binary) {
        if (sv->vtable.codec) {
            return sv->vtable.codec(sv, index);
        } else {
            errno = ENOSYS;
            return NULL;
        }
    } else {
        errno = EINVAL;
        return NULL;
    }
}


/**
signal_annotation
=================

Get an annotation from a signal definition.

Parameters
----------
sv (SignalVector*)
: The Signal Vector object containing the signal.

index (uint32_t)
: Index of the signal in the Signal Vector object.

name (const char*)
: The name of the annotation.

Returns
-------
const char*
: The annotation value.

NULL
: The requested annotation was not found, inspect `errno` for additional
information..

Example (Annotation Specification)
-------

```yaml
kind: SignalGroup
metadata:
  name: data
spec:
  signals:
    - signal: counter
      annotations:
        initial_value: 10
```

Example (Code Usage)
-------

{{< readfile file="../examples/signalvector_annotation.c" code="true" lang="c"
>}}
*/
inline const char* signal_annotation(
    SignalVector* sv, uint32_t index, const char* name)
{
    if (sv && index < sv->count) {
        if (sv->vtable.annotation) {
            return sv->vtable.annotation(sv, index, name);
        } else {
            errno = ENOSYS;
            return NULL;
        }
    } else {
        errno = EINVAL;
        return NULL;
    }
}


/**
signal_group_annotation
=======================

Get an annotation from a signal group.

Parameters
----------
sv (SignalVector*)
: The Signal Vector object representing the signal group.

name (const char*)
: The name of the annotation.

Returns
-------
const char*
: The annotation value.

NULL
: The requested annotation was not found, inspect `errno` for additional
information..
*/
inline const char* signal_group_annotation(SignalVector* sv, const char* name)
{
    if (sv) {
        if (sv->vtable.group_annotation) {
            return sv->vtable.group_annotation(sv, name);
        } else {
            errno = ENOSYS;
            return NULL;
        }
    } else {
        errno = EINVAL;
        return NULL;
    }
}
