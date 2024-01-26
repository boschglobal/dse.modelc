// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <dlfcn.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/schema.h>
#include <dse/modelc/runtime.h>
#include <dse/logger.h>


#define STEP_SIZE     MODEL_DEFAULT_STEP_SIZE
#define END_TIME      3600
#define STEPS         10


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


static void marshal_to_signal_vectors(SignalVector* sv);
static void marshal_from_signal_vectors(SignalVector* sv);
static void print_signal_vectors(SignalVector* sv);


/**
 *  Input parser (YAML doc kind = "Input").
 *
 *  Read signal inputs from a YAML file.
 *
 *  Example
 *  -------
 *      ---
 *      kind: Input
 *      metadata:
 *        name: hym_inst
 *        annotations:
 *          default_channel: test  # Search by name (in this case).
 *      spec:
 *        samples:
 *          - sample: 0.0005
 *            values:
 *              - D_BrakePedal_0_1_f: 0.0  # 0..1
 *              - D_ActMode_1_3_f: 3  # 1 = MC Press, 2 = Pedal Force, 3 = Rod
 * Stroke
 *          - sample: 0.0010
 *            values:
 *             - D_BrakePedal_0_1_f: 0.3
 */
typedef struct ValueObject {
    char*  signal;
    double value;
} ValueObject;

typedef struct SampleObject {
    double  sample_time;
    HashMap values;
    void*   data;  // Sample YAML node.
} SampleObject;

typedef struct Enumerator {
    const char*           path;
    SchemaObject          object;  // Matched YAML doc (object->doc).
    SchemaObjectGenerator generator;
    /* Tracking variables. */
    uint32_t              index;
    SampleObject*         sample;
    SignalVector*         sv;
} Enumerator;

static void* value_object_generator(ModelInstanceSpec* mi, void* data);
static void* sample_object_generator(ModelInstanceSpec* mi, void* data);
static int   setup_enumerator(ModelInstanceSpec* mi, SchemaObject* object);


/**
 *  Model Loader and Stepper.
 *
 *  A debug interface for testing Models in isolation from a SimBus.
 *
 *  Example
 *  -------
 *      $ cd dse/modelc/build/_out/examples/dynamic/
 *      $ ../../bin/mstep \
 *          --name=dynamic_model_instance \
 *          stack.yaml \
 *          signal_group.yaml \
 *          model.yaml
 */
int main(int argc, char** argv)
{
    int                rc;
    ModelCArguments    args;
    SimulationSpec     sim;
    ModelInstanceSpec* mi;

    modelc_set_default_args(&args, NULL, STEP_SIZE, END_TIME);
    args.steps = STEPS;
    modelc_parse_arguments(&args, argc, argv, "Model Loader and Stepper");
    if (args.name == NULL) log_fatal("name argument not provided!");


    /* Configure the Model (takes config from parsed YAML docs). */
    rc = modelc_configure(&args, &sim);
    if (rc) exit(rc);
    mi = modelc_get_model_instance(&sim, args.name);
    if (mi == NULL) log_fatal("ModelInstance %s not found!", args.name);


    /* Loading the Model library and symbols. */
    const char* model_filename = mi->model_definition.full_path;
    log_notice("Loading model: %s ...", model_filename);
    void* handle = dlopen(model_filename, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        log_error("ERROR: dlopen call failed: %s", dlerror());
        log_fatal("Model library not loaded!");
    }
    ModelVTable vtable;
    vtable.create = dlsym(handle, MODEL_CREATE_FUNC_NAME);
    vtable.step = dlsym(handle, MODEL_STEP_FUNC_NAME);
    vtable.destroy = dlsym(handle, MODEL_DESTROY_FUNC_NAME);
    if (vtable.create == NULL && vtable.step == NULL) {
        log_fatal("vtable not complete!");
    }

    /* Call the create function of the Model. */
    rc = modelc_model_create(&sim, mi, &vtable);
    if (rc) {
        log_fatal("Call: model_setup_func failed! (rc=%d)!", rc);
    }
    memcpy(&vtable, &mi->model_desc->vtable, sizeof(ModelVTable));
    SignalVector* sv = mi->model_desc->sv;
    print_signal_vectors(sv);

    /* Load any input data. */
    Enumerator           samples = { .sv = sv };
    SchemaObjectSelector selector = {
        .kind = "Input",
        .name = args.name,
        .data = &samples,
    };
    schema_object_search(mi, &selector, setup_enumerator);
    if (samples.object.doc) {
        /* Load the first sample (for following loop). */
        samples.sample = schema_object_enumerator(mi, &samples.object,
            samples.path, &samples.index, samples.generator);
    }


    /* Run the Model/Simulation. */
    log_notice("Starting Simulation (for %d steps) ...", args.steps);
    double model_time = 0.0;  // no adapter, so fake it.
    for (uint32_t i = 0; i < args.steps; i++) {
        log_notice("  step %d (model_time=%f)", i, model_time);

        /* Set any input signals. */
        if (samples.sample) {
            while (samples.sample->sample_time <= model_time) {
                char** keys = hashmap_keys(&samples.sample->values);
                int    len = hashmap_number_keys(samples.sample->values);
                for (int i = 0; i < len; i++) {
                    double* value =
                        hashmap_get(&samples.sample->values, keys[i]);
                    for (uint32_t j = 0; j < sv->count; j++) {
                        if (strcmp(keys[i], samples.sv->signal[j]) == 0) {
                            log_notice("    input: %s=%f", keys[i], *value);
                            if (samples.sv) samples.sv->scalar[j] = *value;
                            break;
                        }
                    }
                }
                for (int i = 0; i < len; i++) free(keys[i]);
                free(keys);
                hashmap_destroy(&samples.sample->values);
                free(samples.sample);
                // next sample
                samples.sample = NULL;
                samples.sample = schema_object_enumerator(mi, &samples.object,
                    samples.path, &samples.index, samples.generator);
                if (samples.sample == NULL) break;  // no more samples.
            }
        }

        /* Step the model. */
        marshal_to_signal_vectors(sv);
        rc = modelc_step(mi, args.step_size);
        if (rc) log_fatal("Call: modelc_step failed! (i=%d, rc=%d)!", i, rc);
        marshal_from_signal_vectors(sv);
        model_time += args.step_size;
    }
    log_notice("Simulation complete.");
    print_signal_vectors(sv);


    /* Call the exit function of the Model. */
    if (vtable.destroy) {
        vtable.destroy(mi->model_desc);
    }

    exit(0);
}


static void marshal_to_signal_vectors(SignalVector* sv)
{
    while (sv && sv->name) {
        /* Next signal vector. */
        sv++;
    }
}


static void marshal_from_signal_vectors(SignalVector* sv)
{
    while (sv && sv->name) {
        /* Next signal vector. */
        sv++;
    }
}


static void print_signal_vectors(SignalVector* sv)
{
    log_notice("Signal Vectors:");
    while (sv && sv->name) {
        log_notice("  Name: %s", sv->name);
        log_notice("    Model Function: %s", sv->function_name);
        log_notice("    Signal Count  : %d", sv->count);
        if (sv->is_binary) {
            log_notice("    Vector Type   : binary");
            log_notice("    Signals       :");
            for (uint32_t i = 0; i < sv->count; i++) {
                log_notice("      - name : %s", sv->signal[i]);
            }
        } else {
            log_notice("    Vector Type   : double");
            log_notice("    Signals       :");
            for (uint32_t i = 0; i < sv->count; i++) {
                log_notice("      - name : %s", sv->signal[i]);
                log_notice("        value: %g", sv->scalar[i]);
            }
        }
        /* Next signal vector. */
        sv++;
    }
}


static void* value_object_generator(ModelInstanceSpec* mi, void* data)
{
    UNUSED(mi);

    YamlNode* node = data;

    int len = hashmap_number_keys(node->mapping);
    if (len == 0) return NULL;

    char**       keys = hashmap_keys(&node->mapping);
    ValueObject* vo = calloc(1, sizeof(ValueObject));
    vo->signal = keys[0];
    dse_yaml_get_double(node, keys[0], &vo->value);
    /* Free the keys, except for the first entry ... which is returned. */
    for (int i = 1; i < len; i++)
        free(keys[i]);
    free(keys);

    return vo; /* Caller to free, also vo->signal. */
}


static void* sample_object_generator(ModelInstanceSpec* mi, void* data)
{
    UNUSED(mi);

    SampleObject* sample_obj = calloc(1, sizeof(SampleObject));
    dse_yaml_get_double((YamlNode*)data, "sample", &sample_obj->sample_time);
    sample_obj->data = data;
    hashmap_init_alt(&sample_obj->values, 10, NULL);

    /* Enumerate over the values. */
    {
        ValueObject* vo;
        SchemaObject so = { .doc = data };
        uint32_t     index = 0;
        do {
            vo = schema_object_enumerator(
                mi, &so, "values", &index, value_object_generator);
            if (vo == NULL) break;
            hashmap_set_double(&sample_obj->values, vo->signal, vo->value);
            if (vo->signal) free(vo->signal);
            free(vo);
        } while (1);
    }

    return sample_obj; /* Caller to free. */
}


static int setup_enumerator(ModelInstanceSpec* mi, SchemaObject* object)
{
    UNUSED(mi);

    Enumerator* samples = object->data;
    samples->object.doc = object->doc;
    samples->path = "spec/samples";
    samples->index = 0;
    samples->sample = NULL;
    samples->generator = sample_object_generator;

    /* Search for the default signal vector. */
    const char* name = NULL;
    dse_yaml_get_string(
        object->doc, "metadata/annotations/default_channel", &name);
    while (samples->sv && samples->sv->name) {
        if (strcmp(samples->sv->name, name) == 0) break;
        /* Next sv. */
        samples->sv++;
    }

    return 1;  // only use the first match.
}
