// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <dlfcn.h>
#include <dse/testing.h>
#include <dse/platform.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/util/strings.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/mcl_mk1.h>


#define HASHLIST_DEFAULT_SIZE 8
#define GENERAL_BUFFER_LEN    255


static HashList* __mcl_dll_handle =
    NULL; /* Very private collection of MCL DLL handles. */
static HashMap* __mcl_strategy =
    NULL; /* Very private collection of MclStrategyDesc. */
static HashMap* __mcl_adapter =
    NULL; /* Very private collection of MclStrategyDesc. */


static void _allocate_mcl()
{
    if (__mcl_dll_handle == NULL) {
        __mcl_dll_handle = calloc(1, sizeof(HashList));
        hashlist_init(__mcl_dll_handle, 10);
    }
    if (__mcl_strategy == NULL) {
        __mcl_strategy = calloc(1, sizeof(HashMap));
        hashmap_init(__mcl_strategy);
    }
    if (__mcl_adapter == NULL) {
        __mcl_adapter = calloc(1, sizeof(HashMap));
        hashmap_init(__mcl_adapter);
    }
}


void mcl_mk1_register_strategy(MclStrategyDesc* strategy_desc)
{
    assert(__mcl_strategy);
    log_notice("  Adding MCL strategy: %s", strategy_desc->name);
    hashmap_set(__mcl_strategy, strategy_desc->name, strategy_desc);
}


void mcl_mk1_register_adapter(MclAdapterDesc* adapter_desc)
{
    assert(__mcl_adapter);
    log_notice("  Adding MCL adapter: %s", adapter_desc->name);
    hashmap_set(__mcl_adapter, adapter_desc->name, adapter_desc);
}


/**
mcl_mk1_loadlib
===========

Loads an MCL Library which will contain a number of Strategy and/or Adapter
methods. The MCL Library should implement function `mcl_setup()` which will
be called by the MCL when the MCL Libary is loaded. The function `mcl_setup()`
of the MCL Library should call MCL functions `mcl_register_strategy()` and/or
`mcl_register_adapter()` to register _this_ MCL Library with the MCL.

Parameters
----------
model_instance (ModelInstanceSpec*)
: Model Instance object representing the Model. Contains various identifying
  and configuration elements.

Returns
-------
0
: The MCL library way successfully loaded.

Exceptions
----------
exit(errno)
: Any error in loading an MCL represents a fatal configuration error and
  `exit()` is called to terminate execution.
*/
__attribute__((deprecated)) int mcl_mk1_loadlib(
    ModelInstanceSpec* model_instance)
{
    if (__mcl_dll_handle == NULL) _allocate_mcl();
    assert(__mcl_dll_handle);
    log_info("MCL load platform os: %s", PLATFORM_OS);
    log_info("MCL load platform arch: %s", PLATFORM_ARCH);

    /* MCL DLLs are listed in the Model Definition runtime spec according to
       OS/ARCH. */
    assert(model_instance->model_definition.doc);
    const char* selectors[] = { "os" };
    const char* values[] = { PLATFORM_OS };
    YamlNode*   mcl_runtime_node =
        dse_yaml_find_node_in_seq(model_instance->model_definition.doc,
            "spec/runtime/dynlib", selectors, values, 1);
    if (mcl_runtime_node == NULL) {
        if (errno == 0) errno = EINVAL;
        log_fatal("YAML node [Model]/spec/runtime/dynlib[os=%s] not found!",
            PLATFORM_OS);
    }
    YamlNode* mcl_list_node = dse_yaml_find_node(mcl_runtime_node, "libs");
    if (mcl_list_node == NULL) {
        if (errno == 0) errno = EINVAL;
        log_fatal(
            "YAML node [Model]/spec/runtime/dynlib[os=%s]/libs not found!",
            PLATFORM_OS);
    }
    assert(mcl_list_node);

    /* Load each MCL DLL and register. */
    uint32_t mcl_dll_count = hashlist_length(&mcl_list_node->sequence);
    log_info("Loading %u MCL DLL's.", mcl_dll_count);
    for (uint32_t i = 0; i < mcl_dll_count; i++) {
        /* For a simple YAML array, each element is in the "name" property. */
        YamlNode*   dll_node = hashlist_at(&mcl_list_node->sequence, i);
        const char* dll_path = dll_node->name;
        if (dll_path == NULL) {
            log_error("Path not correctly extracted from YAML!");
            continue;
        }
        char* _path =
            dse_path_cat(model_instance->model_definition.path, dll_path);
        log_notice("Loading MCL: %s", _path);
        dlerror(); /* Clear any previous error. */
        void* handle = dlopen(_path, RTLD_NOW | RTLD_LOCAL);
        free(_path);
        if (handle == NULL) {
            log_fatal("ERROR: dlopen call: %s", dlerror());
        }
        assert(handle);
        log_info("Loading symbol: %s ...", MCL_SETUP_FUNC_STR);
        dlerror(); /* Clear any previous error. */
        MclRegisterHandler register_func = dlsym(handle, MCL_SETUP_FUNC_STR);
        char*              dl_error = dlerror();
        if (dl_error != NULL) {
            log_error("ERROR: dlsym call: %s", dl_error);
        }
        /* Call this MCL register method. */
        if (register_func) register_func();
        hashlist_append(__mcl_dll_handle, handle);
    }

    return 0;
}


/**
mcl_mk1_create
==========

Creates an instance of an MCL Model. All configured Model Libraries (of the
MCL Model) are first associated with an MCL Adapter and MCL Strategy (provided
by an MCL Library) and then loaded via the MCL Adapter `load_func()`.

Parameters
----------
model_instance (ModelInstanceSpec*)
: Model Instance object representing the Model. Contains various identifying
  and configuration elements.

Returns
-------
0
: The MCL library way successfully loaded.

Exceptions
----------
exit(errno)
: Any error in creating an MCL Model instance represents a fatal configuration
  error and `exit()` is called to terminate execution.
*/
__attribute__((deprecated)) int mcl_mk1_create(
    ModelInstanceSpec* model_instance)
{
    assert(__mcl_strategy);
    assert(model_instance);
    ModelInstancePrivate* mip = model_instance->private;
    YamlNode*             node;
    MclStrategyDesc*      strategy = NULL;

    YamlNode* mcl_node = dse_yaml_find_node(model_instance->spec, "model/mcl");
    if (mcl_node == NULL) {
        if (errno == 0) errno = EINVAL;
        log_fatal("YAML node [instance]/model/mcl not found!");
    }
    MclInstanceDesc* mcl_instance = calloc(1, sizeof(MclInstanceDesc));
    assert(mcl_instance);
    hashlist_init(&mcl_instance->models, HASHLIST_DEFAULT_SIZE);
    mip->mcl_instance = mcl_instance;
    mcl_instance->model_instance = model_instance;

    /* Find the MCL Strategy. */
    node = dse_yaml_find_node(mcl_node, "strategy");
    if (node && node->scalar) {
        strategy = hashmap_get(__mcl_strategy, node->scalar);
        if (!strategy) {
            log_fatal("MCL strategy not found: %s", node->scalar);
        }
    } else {
        if (errno == 0) errno = EINVAL;
        log_fatal("YAML node mcl/strategy not specified!");
    }
    mcl_instance->strategy = strategy;
    mcl_instance->strategy->mcl_instance = mcl_instance;
    log_notice("Model Compatibility Library:");
    log_notice("  Strategy selected: %s", strategy->name);

    /* Load each listed Model. */
    node = dse_yaml_find_node(mcl_node, "models");
    if (node == NULL) {
        if (errno == 0) errno = EINVAL;
        log_error("YAML node [instance]/model/mcl/models not found!");
    }
    uint32_t _count = hashlist_length(&node->sequence);
    for (uint32_t i = 0; i < _count; i++) {
        const char*     dll_path = NULL;
        const char*     mcl_model_name = NULL;
        MclAdapterDesc* adapter = NULL;
        YamlNode*       model_node = hashlist_at(&node->sequence, i);
        YamlNode*       name_node = dse_yaml_find_node(model_node, "name");
        if (name_node == NULL) {
            if (errno == 0) errno = EINVAL;
            log_error(
                "YAML node [instance]/model/mcl/models[%d] as no name!", i);
        }
        const char* model_name = name_node->scalar;

        /* Find the (actual) Model to be loaded into this MCL Model. */
        const char* selector[] = { "metadata/name" };
        const char* value[] = { model_name };
        YamlNode*   mcl_model_doc = dse_yaml_find_doc_in_doclist(
              model_instance->yaml_doc_list, "Model", selector, value, 1);
        if (mcl_model_doc == NULL) {
            if (errno == 0) errno = EINVAL;
            log_fatal(
                "YAML doc [Model]/metadata/name=%s not found!", model_name);
        }
        /* Find MCL Model metadata. */
        node = dse_yaml_find_node(mcl_model_doc, "metadata/name");
        if (node && node->scalar) {
            mcl_model_name = node->scalar;
            log_debug("metadata/name=%s", mcl_model_name);
        }
        /* Find the MCL Adapter to use with this Model. */
        char      mcl_adapter_name[GENERAL_BUFFER_LEN];
        char      mcl_version_annotation_name[GENERAL_BUFFER_LEN];
        YamlNode* adapter_node = dse_yaml_find_node(
            mcl_model_doc, "metadata/annotations/mcl_adapter");
        if (adapter_node && adapter_node->scalar) {
            snprintf(mcl_version_annotation_name, GENERAL_BUFFER_LEN,
                "metadata/annotations/mcl_%s_version", adapter_node->scalar);
        } else {
            if (errno == 0) errno = EINVAL;
            log_fatal(
                "YAML node metadata/annotations/mcl_adapter not specified!");
        }
        YamlNode* version_node =
            dse_yaml_find_node(mcl_model_doc, mcl_version_annotation_name);
        if (version_node && version_node->scalar) {
            snprintf(mcl_adapter_name, GENERAL_BUFFER_LEN, "%s_%s",
                adapter_node->scalar, version_node->scalar);
        } else {
            if (errno == 0) errno = EINVAL;
            log_fatal(
                "YAML node %s not specified!", mcl_version_annotation_name);
        }

        log_info("Using MCL Adapter: %s", mcl_adapter_name);
        adapter = hashmap_get(__mcl_adapter, mcl_adapter_name);
        if (!adapter) {
            log_fatal("MCL adapter not found: %s", mcl_adapter_name);
        }

        /* Build the MCL Model Descriptor. */
        MclModelDesc* mcl_model;
        mcl_model = calloc(1, sizeof(MclModelDesc));
        // FIXME each MCL should load its own Model DLL.
        if (strncmp(mcl_adapter_name, "fmi", 3) == 0) {
            // FIXME load the FMI Model DLL here until its moved to the FMIMCL.
            const char* selectors[] = { "os" };
            const char* values[] = { PLATFORM_OS };
            YamlNode*   mcl_runtime_node = dse_yaml_find_node_in_seq(
                  mcl_model_doc, "spec/runtime/mcl", selectors, values, 1);
            if (mcl_runtime_node == NULL) {
                if (errno == 0) errno = EINVAL;
                log_fatal(
                    "YAML node [Model]/spec/runtime/mcl[os=%s] not found!",
                    PLATFORM_OS);
            }
            node = dse_yaml_find_node(mcl_runtime_node, "path");
            if (node && node->scalar) {
                dll_path = node->scalar;
            } else {
                if (errno == 0) errno = EINVAL;
                log_fatal("YAML node [Model]/spec/runtime/mcl[os=%s]/path not "
                          "found!",
                    PLATFORM_OS);
            }

            assert(adapter);
            assert(mcl_model_name);
            assert(dll_path);

            /* Load the Model DLL. */
            char* _path =
                dse_path_cat(model_instance->model_definition.path, dll_path);
            log_info("Loading MCL Model: %s", _path);
            dlerror(); /* Clear any previous error. */
            void* dll_handle = dlopen(_path, RTLD_NOW | RTLD_LOCAL);
            if (dll_handle == NULL) {
                log_fatal("ERROR: dlopen call: %s", dlerror());
            }
            assert(dll_handle);

            mcl_model->path = _path;
            mcl_model->handle = dll_handle;
        }
        mcl_model->name = mcl_model_name;
        mcl_model->model_doc = mcl_model_doc;
        mcl_model->adapter = adapter;
        hashlist_append(&mcl_instance->models, mcl_model);
        log_notice("  [%d] MCL Model Name: %s ", i, model_name);
        log_notice("  [%d] Adapter: %s", i, adapter->name);
    }

    return 0;
}


/**
mcl_mk1_destroy
===========

Destroy an instance of an MCL Model, releasing any allocated memory or other
resources allocated to the MCL Model instance.

Parameters
----------
model_instance (ModelInstanceSpec*)
: Model Instance object representing the Model. Contains various identifying
  and configuration elements.
*/
__attribute__((deprecated)) void mcl_mk1_destroy(
    ModelInstanceSpec* model_instance)
{
    if (model_instance && model_instance->private) {
        ModelInstancePrivate* mip = model_instance->private;
        MclInstanceDesc*      mcl_instance = mip->mcl_instance;
        if (mcl_instance) {
            for (uint32_t i = 0; i < hashlist_length(&mcl_instance->models);
                 i++) {
                MclModelDesc* mcl_model = hashlist_at(&mcl_instance->models, i);
                free(mcl_model->path);
                free(mcl_model);
            }
            hashlist_destroy(&mcl_instance->models);
            free(mcl_instance);
            mip->mcl_instance = NULL;
        }
    }
}


/**
mcl_mk1_register_strategy
=====================

An MCL Library calls this function to register a particular MCL Strategy with
the MCL.

Parameters
----------
strategy (MclStrategyDesc*)
: MCL Strategy object representing a strategy capability of the MCL Library.
*/
extern void mcl_mk1_register_strategy(MclStrategyDesc* strategy);


/**
mcl_mk1_register_adapter
====================

An MCL Library calls this function to register a particular MCL Adapter with
the MCL.

Parameters
----------
adapter (MclAdapterDesc*)
: MCL Strategy object representing a strategy capability of the MCL Library.
*/
extern void mcl_mk1_register_adapter(MclAdapterDesc* adapter);
