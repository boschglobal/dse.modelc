// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_SCHEMA_H_
#define DSE_MODELC_SCHEMA_H_

#include <stdint.h>
#include <dse/platform.h>
#include <dse/modelc/runtime.h>


#ifndef DLL_PUBLIC
#define DLL_PUBLIC  __attribute__((visibility("default")))
#endif
#ifndef DLL_PRIVATE
#define DLL_PRIVATE __attribute__((visibility("hidden")))
#endif


/**
Schema API
==========

The Schema API provides a number of functions for parsing the YAML configuration
documents which represent (some of) the Schemas of the Model C Library.
*/

typedef struct SchemaObject {
    const char* kind;
    const char* name;
    void*       doc;
    /* Data passed from schema_object_search (via SchemaObjectSelector). */
    void*       data;
} SchemaObject;


typedef struct SchemaLabel {
    const char* name;
    const char* value;
} SchemaLabel;


typedef struct SchemaObjectSelector {
    const char*  kind;
    const char*  name;
    SchemaLabel* labels;
    int          labels_len;
    /* Data passed to match handler (via SchemaObject). */
    void*        data;
} SchemaObjectSelector;


/* schema.c - Schema Interface. */
typedef int (*SchemaMatchHandler)(
    ModelInstanceSpec* model_instance, SchemaObject* object);
typedef void* (*SchemaObjectGenerator)(
    ModelInstanceSpec* model_instance, void* data);

DLL_PUBLIC int schema_object_search(ModelInstanceSpec* model_instance,
    SchemaObjectSelector* selector, SchemaMatchHandler handler);
DLL_PUBLIC SchemaObjectSelector* schema_build_channel_selector(
    ModelInstanceSpec* model_instance, ChannelSpec* channel, const char* kind);
DLL_PUBLIC void  schema_release_selector(SchemaObjectSelector* selector);
DLL_PUBLIC void* schema_object_enumerator(ModelInstanceSpec* model_instance,
    SchemaObject* object, const char* path, uint32_t* index,
    SchemaObjectGenerator generator);


/* schema.c - Schema Object Generators. */
typedef struct SchemaSignalObject {
    const char* signal;
    /* The underlying schema object (i.e. YAML Node). */
    void*       data;
} SchemaSignalObject;


DLL_PUBLIC void* schema_signal_object_generator(
    ModelInstanceSpec* model_instance, void* data);


#endif  // DSE_MODELC_SCHEMA_H_
