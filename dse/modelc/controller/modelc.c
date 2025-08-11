// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/platform.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/util/strings.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/modelc/controller/controller.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/modelc/model.h>


#define UNUSED(x)     ((void)x)

/* CLI related defaults. */
#define MODEL_TIMEOUT 60


static double __stop_request = 0; /* Very private, indicate stop request. */

extern ModelSignalIndex __model_index__(
    ModelDesc* m, const char* vname, const char* sname);


static int _destroy_model_function(void* mf, void* additional_data)
{
    UNUSED(additional_data);

    /* This calls free() on the _mf object, add to hash with hashmap_set(). */
    model_function_destroy((ModelFunction*)mf);
    return 0;
}


static void _destroy_model_instances(SimulationSpec* sim)
{
    ModelInstanceSpec* _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        ModelInstancePrivate* mip = _instptr->private;
        free(_instptr->name);
        free(_instptr->model_definition.full_path);
        if (_instptr->model_desc) {
            if (_instptr->model_desc->sv)
                model_sv_destroy(_instptr->model_desc->sv);
            free(_instptr->model_desc);
        }
        /* ControllerModel */
        ControllerModel* cm = mip->controller_model;
        if (cm) {
            HashMap* mf_map = &cm->model_functions;
            hashmap_iterator(mf_map, _destroy_model_function, true, NULL);
            hashmap_destroy(mf_map);
            if (cm->handle) dlclose(cm->handle);
            free(cm);
            cm = NULL;
        }
        /* Adapter Model. */
        AdapterModel* am = mip->adapter_model;
        if (am) {
            adapter_destroy_adapter_model(am);
            am = NULL;
        }

        /* Private data. */
        free(mip);

        /* Next instance? */
        _instptr++;
    }
    free(sim->instance_list);
}

static char* _dse_path_cat(const char* a, const char* b)
{
    if (a == NULL && b == NULL) return NULL;

    /* Caller will free. */
    int len = 2;  // '/' + NULL.
    len += (a) ? strlen(a) : 0;
    len += (b) ? strlen(b) : 0;
    char* path = calloc(len, sizeof(char));

    if (a && b) {
        snprintf(path, len, "%s/%s", a, b);
    } else {
        strncpy(path, a ? a : b, len - 1);
    }

    return path;
}

int modelc_configure_model(
    ModelCArguments* args, ModelInstanceSpec* model_instance)
{
    assert(args);
    assert(model_instance);
    errno = 0;

    YamlNode*   mi_node;
    YamlNode*   md_doc;
    YamlNode*   node;
    const char* model_name = NULL;

    /* Model Instance: locate in stack. */
    mi_node = dse_yaml_find_node_in_seq_in_doclist(args->yaml_doc_list, "Stack",
        "spec/models", "name", model_instance->name);
    if (mi_node == NULL) {
        if (errno == 0) errno = EINVAL;
        log_error(
            "Model Instance (%s) not found in Stack!", model_instance->name);
        return errno;
    }
    model_instance->spec = mi_node;
    /* UID, if not set (0) will be assigned by SimBus. */
    if (model_instance->uid == 0) {
        node = dse_yaml_find_node(mi_node, "uid");
        if (node) model_instance->uid = strtoul(node->scalar, NULL, 10);
    }
    /* Name. */
    node = dse_yaml_find_node(mi_node, "model/name");
    if (node && node->scalar) {
        model_name = node->scalar;
    } else {
        if (errno == 0) errno = EINVAL;
        log_error("Model Definition not found!");
        return errno;
    }
    model_instance->model_definition.name = model_name;
    /* Path. */
    node = dse_yaml_find_node(mi_node, "model/metadata/annotations/path");
    if (node && node->scalar) {
        model_instance->model_definition.path = node->scalar;
        /* Load and add the Model Definition to the doc list. */
        char* md_file =
            _dse_path_cat(model_instance->model_definition.path, "model.yaml");
        log_notice("Load YAML File: %s", md_file);
        args->yaml_doc_list = dse_yaml_load_file(md_file, args->yaml_doc_list);
        free(md_file);
    }
    /* Model Definition. */
    const char* selector[] = { "metadata/name" };
    const char* value[] = { model_name };
    md_doc = dse_yaml_find_doc_in_doclist(
        args->yaml_doc_list, "Model", selector, value, 1);
    if (md_doc) {
        model_instance->model_definition.doc = md_doc;
        /* Filename (of the dynlib) */
        const char* selectors[] = { "os", "arch" };
        const char* values[] = { PLATFORM_OS, PLATFORM_ARCH };
        YamlNode*   dl_node;

        dl_node = dse_yaml_find_node_in_seq(
            md_doc, "spec/runtime/dynlib", selectors, values, 2);
        if (dl_node) {
            node = dse_yaml_find_node(dl_node, "path");
            if (node && node->scalar) {
                model_instance->model_definition.file = node->scalar;
            } else {
                log_fatal("Model path not found in Model Definition!");
            }
        }
    }

    /* CLI overrides, development use case (normally take from stack). */
    if (args->file) model_instance->model_definition.file = args->file;
    if (args->path) model_instance->model_definition.path = args->path;

    /* Final adjustments. */
    if (model_instance->model_definition.file) {
        model_instance->model_definition.full_path =
            _dse_path_cat(model_instance->model_definition.path,
                model_instance->model_definition.file);
    }

    /* Set a reference ot the parsed YAML Doc List.  */
    model_instance->yaml_doc_list = args->yaml_doc_list;

    log_notice("Model Instance:");
    log_notice("  Name: %s", model_instance->name);
    log_notice("  UID: %u", model_instance->uid);
    log_notice("  Model Name: %s", model_instance->model_definition.name);
    log_notice("  Model Path: %s", model_instance->model_definition.path);
    log_notice("  Model File: %s", model_instance->model_definition.file);
    log_notice(
        "  Model Location: %s", model_instance->model_definition.full_path);

    return 0;
}


#define MODEL_NAME_SEP ";,"

int modelc_configure(ModelCArguments* args, SimulationSpec* sim)
{
    assert(args);
    assert(sim);
    errno = 0;

    int model_count = 0;
    {
        char* model_names = strdup(args->name);
        char* _saveptr = NULL;
        char* _nameptr = strtok_r(model_names, MODEL_NAME_SEP, &_saveptr);
        while (_nameptr) {
            model_count++;
            _nameptr = strtok_r(NULL, MODEL_NAME_SEP, &_saveptr);
        }
        free(model_names);
    }
    log_trace("Parsed %d model names from %s", model_count, args->name);
    if (model_count == 0) {
        log_error("No model names parsed from arg (or stack): %s", args->name);
        errno = EINVAL;
        return -1;
    }
    sim->instance_list = calloc(model_count + 1, sizeof(ModelInstanceSpec));

    /* Configure the Simulation spec. */
    sim->transport = args->transport;
    sim->uri = args->uri;
    sim->uid = args->uid;
    sim->timeout = args->timeout;
    sim->step_size = args->step_size;
    sim->end_time = args->end_time;
    sim->sim_path = args->sim_path;
    dse_yaml_get_bool(modelc_find_stack(args), "spec/runtime/sequential",
        &sim->sequential_cosim);

    log_notice("Simulation Parameters:");
    log_notice("  Step Size: %f", sim->step_size);
    log_notice("  End Time: %f", sim->end_time);
    log_notice("  Model Timeout: %f", sim->timeout);
    log_notice("  Sim Path: %s", sim->sim_path);
    log_notice("  Stacked: %s", model_count > 1 ? "yes" : "no");
    log_notice("  Sequential: %s", sim->sequential_cosim ? "yes" : "no");

    log_notice("Transport:");
    log_notice("  Transport: %s", sim->transport);
    log_notice("  URI: %s", sim->uri);

    log_notice("Platform:");
    log_notice("  Platform OS: %s", PLATFORM_OS);
    log_notice("  Platform Arch: %s", PLATFORM_ARCH);

    /* Sanity-check any configuration. */
    if (sim->timeout <= 0) sim->timeout = MODEL_TIMEOUT;
    if (sim->step_size > sim->end_time) {
        log_fatal("Step Size is greater than End Time!");
    }

    /* Configure the Instance objects. */
    ModelInstanceSpec* _instptr = sim->instance_list;
    {
        char* model_names = strdup(args->name);
        char* _saveptr = NULL;
        char* _nameptr = strtok_r(model_names, MODEL_NAME_SEP, &_saveptr);
        while (_nameptr) {
            _instptr->private = calloc(1, sizeof(ModelInstancePrivate));
            ModelInstancePrivate* mip = _instptr->private;
            assert(_instptr->private);
            _instptr->name = strdup(_nameptr);
            modelc_configure_model(args, _instptr);

            /* Allocate a Controller Model object. */
            int rc;
            mip->controller_model = calloc(1, sizeof(ControllerModel));
            rc = hashmap_init(&mip->controller_model->model_functions);
            if (rc) {
                if (errno == 0) errno = ENOMEM;
                log_fatal("Hashmap init failed for model_functions!");
            }
            /* Allocate a Adapter Model object. */
            mip->adapter_model = calloc(1, sizeof(AdapterModel));
            rc = hashmap_init(&mip->adapter_model->channels);
            if (rc) {
                if (errno == 0) errno = ENOMEM;
                log_fatal("Hashmap init failed for channels!");
            }

            /* Next instance? */
            _nameptr = strtok_r(NULL, MODEL_NAME_SEP, &_saveptr);
            _instptr++;
        }
        free(model_names);
    }

    return 0;
}


static int _configure_model_channel(YamlNode* ch_node, ModelInstanceSpec* mi)
{
    const char* name;

    /* Locate the channel by alias name. */
    dse_yaml_get_string(ch_node, "alias", &name);
    if (name == NULL) {
        /* Fallback to name. */
        dse_yaml_get_string(ch_node, "name", &name);
        if (name == NULL) {
            errno = EINVAL;
            log_error("Could not find channel alias or name!");
            return -EINVAL;
        }
    }

    /* Configure the channel. */
    int rc = model_configure_channel(mi, name, MODEL_STEP_FUNC_NAME);
    if (rc != 0) {
        if (errno == 0) errno = rc;
        log_error("Channel could not be configured! Check stack alias.");
        return -EINVAL;
    }

    return 0;
}


static int _model_function_register(
    ModelInstanceSpec* model_instance, const char* name, double step_size)
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

int modelc_model_create(
    SimulationSpec* sim, ModelInstanceSpec* mi, ModelVTable* model_vtable)
{
    /* Create the Model Function (to represent model_step). */
    if (model_vtable->step == NULL) {
        errno = EINVAL;
        log_error("Model has no " MODEL_STEP_FUNC_NAME "() function");
        return -errno;
    }
    int rc = _model_function_register(mi, MODEL_STEP_FUNC_NAME, sim->step_size);
    if (rc != 0) {
        if (errno == 0) errno = rc;
        log_error("Model function registration failed!");
        return rc;
    }

    /* Setup the channels and signal vector. */
    YamlNode* c_node;
    if (dse_yaml_find_node(mi->model_definition.doc, "spec/runtime/gateway")) {
        log_debug("Select channels based on Model Instance (Gateway)");
        c_node = dse_yaml_find_node(mi->spec, "channels");
    } else {
        c_node = dse_yaml_find_node(mi->model_definition.doc, "spec/channels");
    }
    if (c_node) {
        for (uint32_t i = 0; i < hashlist_length(&c_node->sequence); i++) {
            YamlNode* ch_node = hashlist_at(&c_node->sequence, i);
            int       rc = _configure_model_channel(ch_node, mi);
            if (rc) {
                if (errno == 0) errno = EINVAL;
                log_error("Model has no " MODEL_STEP_FUNC_NAME "() function");
                return -errno;
            }
        }
    }
    SignalVector* sv = model_sv_create(mi);

    /* Setup the initial ModelDesc object. */
    ModelDesc* model_desc = calloc(1, sizeof(ModelDesc));
    memcpy(&model_desc->vtable, model_vtable, sizeof(ModelVTable));
    model_desc->index = model_desc->vtable.index = __model_index__;
    model_desc->sim = sim;
    model_desc->mi = mi;
    model_desc->sv = sv;

    /* Call create (if it exists). */
    if (model_desc->vtable.create) {
        errno = 0;
        ModelDesc* extended_model_desc = model_desc->vtable.create(model_desc);
        if (errno) {
            log_error("Error condition while calling " MODEL_CREATE_FUNC_NAME);
        }
        if (extended_model_desc) {
            if (extended_model_desc != model_desc) {
                /* The model returned an extended (new) ModelDesc object. */
                free(model_desc);
                model_desc = extended_model_desc;
            }
        }
        /* Reset some elements of the ModelDesc (don't trust the model). */
        model_desc->sim = sim;
        model_desc->mi = mi;
        model_desc->sv = sv;
    }

    /* Finalise the ModelDesc object. */
    mi->model_desc = model_desc;
    return 0;
}


/**
 *  modelc_get_model_instance
 *
 *  Find the ModelInstanceSpec object representing the model instance with the
 *  given name in the referenced SimulationSpec* object.
 *
 *  Parameters
 *  ----------
 *  sim : SimulationSpec (pointer to)
 *      Simulation specification (contains all Model Instance objects).
 *  name : const char*
 *      Name of the requested model instance.
 *
 *  Returns
 *  -------
 *      ModelInstanceSpec (pointer to) : First model instance, whose property
 *  `name` is equal to the parameter `name`. NULL : No matching
 * ModelInstanceSpec object found.
 */
ModelInstanceSpec* modelc_get_model_instance(
    SimulationSpec* sim, const char* name)
{
    ModelInstanceSpec* _instptr = sim->instance_list;
    while (_instptr && _instptr->name) {
        if (strcmp(_instptr->name, name) == 0) return _instptr;
        /* Next instance. */
        _instptr++;
    }
    return NULL;
}


static Endpoint* _create_endpoint(SimulationSpec* sim)
{
    int       retry_count = 60;
    Endpoint* endpoint = NULL;

    while (--retry_count) {
        endpoint = endpoint_create(
            sim->transport, sim->uri, sim->uid, false, sim->timeout);
        if (endpoint) break;
        if (__stop_request) {
            /* Early stop request, only would occur if endpoint creation
               was failing due to misconfiguration. */
            errno = ECANCELED;
            log_fatal("Signaled!");
        }
        sleep(1);
        log_notice("Retry endpoint creation ...");
    }

    return endpoint;
}


int modelc_run(SimulationSpec* sim, bool run_async)
{
    assert(sim);
    errno = 0;

    /* Create Endpoint object. */
    log_notice("Create the Endpoint object ...");
    Endpoint* endpoint = _create_endpoint(sim);
    if (endpoint == NULL) {
        log_fatal("Could not create endpoint!");
    }

    /* Setup Sim UID. */
    if (sim->uid == 0) sim->uid = endpoint->uid;
    for (ModelInstanceSpec* _instptr = sim->instance_list;
         _instptr && _instptr->name; _instptr++) {
        if (_instptr->uid) {
            /* Set Sim UID to that of the first specified Model. */
            sim->uid = _instptr->uid;
            break;
        }
    }
    log_debug("sim->uid = %d", sim->uid);
    /* Setup Model UIDs. */
    int inst_counter = 0;
    for (ModelInstanceSpec* _instptr = sim->instance_list;
         _instptr && _instptr->name; _instptr++) {
        /* Generate a UID for this Model. */
        uint32_t _uid = (inst_counter * 10000) + sim->uid;
        if (_instptr->uid == 0) _instptr->uid = _uid;
        log_debug("mi[%d]->uid = %d", inst_counter, _instptr->uid);
        inst_counter++;
    }

    /* Create Controller object. */
    log_notice("Create the Controller object ...");
    controller_init(endpoint, sim);

    /* Load all Simulation Models. */
    log_notice("Load and configure the Simulation Models ...");
    int rc = controller_load_models(sim);
    if (rc) log_fatal("Error loading Simulation Models!");

    /* Run async? */
    if (run_async == true) {
        log_notice("Setup for async Simulation Model run ...");
        controller_bus_ready(sim);
        return 0;
    }

    /* Otherwise, handover to the controller and do synchronous run. */
    log_notice("Run the Simulation ...");
    errno = 0;
    controller_run(sim);

    if (errno == ECANCELED) return errno;
    return 0; /* Caller can inspect errno to determine any conditions. */
}


int modelc_sync(SimulationSpec* sim)
{
    assert(sim);

    /* Async operation:
     * Models simulation environemnt manually syncs.
     * do_step() callbacks for each model function which activates, indicates
     * to models simulation environemnt _which_ model functions should run as
     * well as the start and stop times from the next step. */
    errno = 0;
    if (sim->mode_loopback) {
        return controller_step_phased(sim);
    } else {
        return controller_step(sim);
    }
}


void modelc_shutdown(SimulationSpec* sim)
{
    /* Request and exit from the run loop.
       NOTICE: This is called from interrupt code, only set variables, then
       let controller_run() exit by itself.
    */
    __stop_request = 1;
    controller_stop(sim);
}


void modelc_exit(SimulationSpec* sim)
{
    controller_dump_debug(sim);
    controller_exit(sim);
    _destroy_model_instances(sim);
}
