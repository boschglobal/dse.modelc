// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <yaml.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/model.h>
#include <dse/modelc/schema.h>


#define UNUSED(x) ((void)x)


/**
schema_object_search
====================

Search the collection of schema objects according to the selector, and
call the handler function for each matching object. Schema objects are
searched in the order they were parsed (i.e. listed order at the CLI).

Example
-------

{{< readfile file="../examples/schema_object_search.c" code="true" lang="c" >}}

Parameters
----------
model_instance (ModelInstanceSpec*)
: The Model Instance, which holds references to the various schema objects
  which will be searched.

selector (SchemaObjectSelector*)
: A selector object, schema objects are matched as follows:
   * kind - matches the object kind (if NULL matches _all_ object kinds).
   * name - matches the object name (if NULL matches _all_ object names).
   * labels[] - matches _all_ labels of the object (i.e. _AND_).

handler (SchemaMatchHandler)
: A handler function which is called for each matching schema object. The
  handler can control the search continuation by returning as follows:
   * 0 - the search should continue until no more matches are found.
   * +ve -  the search should stop and return 0 (indicating success).
   * -ve - the search should abort, set errno with a value to indicate
     the failing condition, and return +ve (indicating failure).

Returns
-------
0
: The schema search successfully completed.

+ve
: Failure, inspect errno for an indicator of the failing condition.
*/
int schema_object_search(ModelInstanceSpec* model_instance,
    SchemaObjectSelector* selector, SchemaMatchHandler handler)
{
    assert(model_instance);
    assert(selector);
    assert(handler);
    YamlDocList* doc_list = model_instance->yaml_doc_list;
    YamlNode*    doc;
    YamlNode*    node;

    if (handler == NULL) return 0;
    if (doc_list == NULL) return 0;

    for (uint32_t i = 0; i < hashlist_length(doc_list); i++) {
        log_debug("  searching document ...");
        doc = hashlist_at(doc_list, i);
        SchemaObject object = { 0 };

        /* Kind */
        if (selector->kind) {
            node = dse_yaml_find_node(doc, "kind");
            if (node == NULL || node->scalar == NULL) continue;
            if (strcmp(node->scalar, selector->kind) != 0) continue;
            /* Match on kind. */
            object.kind = node->scalar;
        }
        /* Name */
        if (selector->name) {
            node = dse_yaml_find_node(doc, "metadata/name");
            if (node == NULL || node->scalar == NULL) continue;
            if (strcmp(node->scalar, selector->name) != 0) continue;
            /* Match on name. */
            object.name = node->scalar;
        }
        /* Labels */
        if (selector->labels && selector->labels_len) {
            YamlNode* labels = dse_yaml_find_node(doc, "metadata/labels");
            if (labels == NULL) continue;
            int label_match_count = 0;
            for (int j = 0; j < selector->labels_len; j++) {
                if (selector->labels[j].name == NULL) continue;
                if (selector->labels[j].value == NULL) continue;
                node = dse_yaml_find_node(labels, selector->labels[j].name);
                if (node == NULL || node->scalar == NULL) continue;
                if (strcmp(node->scalar, selector->labels[j].value) != 0) {
                    log_debug("  non-match on label %s: %s (looking for %s)",
                        selector->labels[j].name, node->scalar,
                        selector->labels[j].value);
                    continue;
                }
                /* Match on labels[j]. */
                log_debug("  match on label %s: %s", selector->labels[j].name,
                    node->scalar);
                label_match_count++;
            }
            if (label_match_count != selector->labels_len) continue;
        }
        /* All match conditions of the selector were satisfied! */
        log_debug("  all match conditions, call match handler");
        object.doc = (void*)doc;
        object.data = (void*)selector->data;
        int rc = handler(model_instance, &object);
        if (rc != 0) return 1;
    }

    return 0;
}


/**
schema_build_channel_selector
=============================

Builds a channel selector object based on the provided Model Instance and
Channel Spec. The caller should free the object by calling
`schema_release_channel_selector()`.

The returned SchemaObjectSelector can be used when calling
schema_object_search() to search for schema objects.

> Note: A channel selector will not match on metadata/name.

Parameters
----------
model_instance (ModelInstanceSpec*)
: The Model Instance, which holds references to the various schema objects
  that will searched by the generated selector.

channel (ChannelSpec*)
: A channel spec object.

kind (const char*)
: The kind of schema object to select.

Returns
-------
SchemaObjectSelector (pointer)
: The complete selector object.

NULL
: A selector object could not be created. This return value does not represent
  an error condition. The caller will determine if this condition represents
  and error (typically a configuration error).
 */
SchemaObjectSelector* schema_build_channel_selector(
    ModelInstanceSpec* model_instance, ChannelSpec* channel, const char* kind)
{
    assert(model_instance);
    assert(channel);
    YamlNode* channel_node = (YamlNode*)channel->private;
    YamlNode* selectors_node = NULL;

    /* Selector of the Model Instance Channel has priority. */
    selectors_node = dse_yaml_find_node(channel_node, "selectors");
    if (selectors_node && selectors_node->node_type == YAML_MAPPING_NODE) {
        if (hashmap_number_keys(selectors_node->mapping) == 0) {
            /* Empty selectors node, trigger search on the Model. */
            log_debug(
                "Selectors node is empty, no labels, searching on Model.");
            selectors_node = NULL;
        } else {
            log_debug("Selectors node identified on Model Instance.");
        }
    }
    if (selectors_node == NULL) {
        /* Otherwise locate the Selector on the Model. */
        YamlNode* model_ch_node = NULL;
        log_debug("channel->name = %s", channel->name);
        log_debug("channel->alias = %s", channel->alias);
        if (channel->name) {
            const char* selectors[] = { "name" };
            const char* values[] = { channel->name };
            model_ch_node =
                dse_yaml_find_node_in_seq(model_instance->model_definition.doc,
                    "spec/channels", selectors, values, 1);
        }
        if (model_ch_node == NULL && channel->alias) {
            const char* selectors[] = { "alias" };
            const char* values[] = { channel->alias };
            model_ch_node =
                dse_yaml_find_node_in_seq(model_instance->model_definition.doc,
                    "spec/channels", selectors, values, 1);
        }
        if (model_ch_node) {
            selectors_node = dse_yaml_find_node(model_ch_node, "selectors");
            if (selectors_node) {
                log_debug("Selectors node identified on Model.");
            }
        }
    }
    if (selectors_node == NULL) {
        log_debug("Selectors node not found.");
        return NULL;
    }
    if (selectors_node->node_type != YAML_MAPPING_NODE) {
        log_error("Selectors node is not a mapping!");
        return NULL;
    }

    /* Parse the selectors. */
    int selector_count = hashmap_number_keys(selectors_node->mapping);
    if (selector_count == 0) {
        log_debug("Selectors node is empty, no labels.");
        return NULL;
    }
    log_debug("Selectors:");
    SchemaObjectSelector* selector = calloc(1, sizeof(SchemaObjectSelector));
    selector->labels = calloc(selector_count, sizeof(SchemaLabel));
    selector->labels_len = selector_count;
    selector->kind = kind;
    char** _keys = hashmap_keys(&selectors_node->mapping);
    for (int i = 0; i < selector_count; i++) {
        YamlNode* _n = hashmap_get(&selectors_node->mapping, _keys[i]);
        if (_n == NULL || _n->node_type != YAML_SCALAR_NODE) continue;
        selector->labels[i].name = _n->name;
        selector->labels[i].value = _n->scalar;
        log_debug("  %s = %s", _n->name, _n->scalar);
    }
    for (int _ = 0; _ < selector_count; _++)
        free(_keys[_]);
    free(_keys);

    return selector; /* Caller to free. */
}


/**
schema_release_selector
=======================

Release any allocated memory in a SchemaObjectSelector object.

Parameters
----------
selector (SchemaObjectSelector*)
: A selector object, created by calling `schema_build_channel_selector()`.
*/
void schema_release_selector(SchemaObjectSelector* selector)
{
    if (selector) {
        if (selector->labels) free(selector->labels);
        free(selector);
    }
}


/**
schema_object_enumerator
========================

Enumerate over all child objects of a schema list object. Each child object
is marshalled via the generator function and returned to the caller.

When index exceeds the length of the schema list object the function
returns NULL and the enumeration is complete.

Parameters
----------
model_instance (ModelInstanceSpec*)
: The Model Instance, which holds references to the various schema objects
  which will be enumerated over..

object (SchemaObject*)
: The schema list object to enumerate over.

path (const char*)
: Enumerate objects located at this path, relative from the `object`.

index (uint32_t*)
: Maintains the enumerator postion between calls. Set to 0 to begin a new
  schema object enumeration (i.e. from the first object in the list).

generator (SchemaObjectGenerator)
: A generator function which creates the required schema object.

Returns
-------
void*
: Pointer to the generated object created by the `generator` function. The
  caller must free this object.

NULL
: The enumeration is complete.
 */
void* schema_object_enumerator(ModelInstanceSpec* model_instance,
    SchemaObject* object, const char* path, uint32_t* index,
    SchemaObjectGenerator generator)
{
    assert(object);
    assert(object->doc);
    if (object == NULL) return NULL;
    if (object->doc == NULL) return NULL;

    YamlNode* doc = object->doc;
    YamlNode* node = dse_yaml_find_node(doc, path);
    if (node == NULL || node->node_type != YAML_SEQUENCE_NODE) return NULL;
    if (*index >= hashlist_length(&node->sequence)) return NULL;

    /* Generate the object, and return. */
    void* o = generator(model_instance, hashlist_at(&node->sequence, *index));
    *index = *index + 1;
    return o; /* Caller to free. */
}


/**
schema_signal_object_generator
==============================

Generate a schema signal object.

Parameters
----------
model_instance (ModelInstanceSpec*)
: The Model Instance, which holds references to the various schema objects
  which will be searched.

data (void*)
: The YAML node to generate from.

Returns
-------
void*
: Pointer to the generated schema signal object. The caller must free
  this object.

NULL
: The object cannot be generated.
 */
void* schema_signal_object_generator(
    ModelInstanceSpec* model_instance, void* data)
{
    UNUSED(model_instance);
    YamlNode* n = dse_yaml_find_node((YamlNode*)data, "signal");
    if (n && n->scalar) {
        SchemaSignalObject* o = calloc(1, sizeof(SchemaSignalObject));
        o->signal = n->scalar;
        o->data = data;
        return o; /* Caller to free. */
    }
    return NULL;
}


/**
schema_load_object
==================

Load the specified fields from the YAML node and marshal them into the
provided object. The field spec (`SchemaFieldSpec`) lists the type, path and
offset for the fields to be loaded. Additionally a field map spec
(`SchemaFieldMapSpec`) supports mapping/enumerations values.

> Hint: The field map spec is a NTL (Null-Terminated-List).

Example
-------

{{< readfile file="../examples/schema_load_object.c" code="true" lang="c" >}}

Parameters
----------
node (void*)
: The YAML node to load fields from.

node (void*)
: The object where fields should be loaded to.

spec (SchemaFieldSpec)
: Specification of fields to load from the `node`.

count (size_t)
: Number of items in the `spec`.
*/
void schema_load_object(
    void* node, void* object, const SchemaFieldSpec* spec, size_t count)
{
    YamlNode* n = (YamlNode*)node;
    uint8_t*  o = (uint8_t*)object;
    int       _;

    log_debug("Schema load object:");

    for (size_t i = 0; i < count; i++) {
        const SchemaFieldSpec* s = &spec[i];
        switch (s->type) {
        case SchemaFieldTypeU8: {
            if (s->map) {
                const char* v = NULL;
                dse_yaml_get_string(n, s->path, &v);
                if (v == NULL) continue;
                for (const SchemaFieldMapSpec* m = s->map; m && m->key; m++) {
                    if (strcmp(v, m->key) == 0) {
                        *(uint8_t*)(o + s->offset) = (uint8_t)m->val;
                        log_debug("  load field: %s=%u (%s)", s->path,
                            (uint8_t)m->val, m->key);
                        break;
                    }
                }
                continue;
            } else {
                unsigned int _uint = 0;
                _ = dse_yaml_get_uint(n, s->path, &_uint);
                *(uint8_t*)(o + s->offset) = (uint8_t)_uint;
                if (!_)
                    log_debug("  load field: %s=%u", s->path, (uint8_t)_uint);
            }
            break;
        }
        case SchemaFieldTypeU16: {
            unsigned int _uint = 0;
            _ = dse_yaml_get_uint(n, s->path, &_uint);
            *(uint16_t*)(o + s->offset) = (uint16_t)_uint;
            if (!_) log_debug("  load field: %s=%u", s->path, (uint8_t)_uint);
            break;
        }
        case SchemaFieldTypeU32: {
            unsigned int _uint = 0;
            _ = dse_yaml_get_uint(n, s->path, &_uint);
            *(uint32_t*)(o + s->offset) = (uint32_t)_uint;
            if (!_) log_debug("  load field: %s=%u", s->path, (uint32_t)_uint);
            break;
        }
        case SchemaFieldTypeD: {
            _ = dse_yaml_get_double(n, s->path, (double*)(o + s->offset));
            if (!_)
                log_debug(
                    "  load field: %s=%f", s->path, *(double*)(o + s->offset));
            break;
        }
        case SchemaFieldTypeB: {
            _ = dse_yaml_get_bool(n, s->path, (bool*)(o + s->offset));
            if (!_)
                log_debug(
                    "  load field: %s=%u", s->path, *(bool*)(o + s->offset));
            break;
        }
        case SchemaFieldTypeS: {
            _ = dse_yaml_get_string(n, s->path, (const char**)(o + s->offset));
            if (!_)
                log_debug("  load field: %s=%s", s->path,
                    *(const char**)(o + s->offset));
            break;
        }
        default:
            log_debug("  Warning, unsupported field type (type=%u)", s->type);
        }
    }
}
