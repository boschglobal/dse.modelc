// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/adapter/adapter.h>
#include <dse/modelc/adapter/simbus/simbus.h>
#include <dse/modelc/adapter/transport/endpoint.h>
#include <dse/modelc/runtime.h>
#include <dse/logger.h>


#define ARRAY_LENGTH(array) (sizeof((array)) / sizeof((array)[0]))


/* CLI related defaults. */
#define CLI_DOC             "Standalone SimBus"
#define TRANSPORT           TRANSPORT_REDISPUBSUB
#define BUS_STEP_SIZE       0.005
#define BUS_MODEL_UID       8000008
#define BUS_TIMEOUT         1 /* This is the wait_message timeout. */
#define MODEL_NAME          "simbus"


/* SimBus main program entry point. */
int main(int argc, char** argv)
{
    setbuf(stdout, NULL);

    ModelCArguments args = {0};

    /* Additional bookkeeping data. */
    log_notice("Version: %s", MODELC_VERSION);

    /* Set default arguments and then parse from CLI. */
    modelc_set_default_args(&args, MODEL_NAME, BUS_STEP_SIZE, 0);
    /* Special Bus Mode settings (for SimBus operation). */
    args.timeout = BUS_TIMEOUT;
    args.timeout_set_by_cli = 1; /* Ignore any value in the YAML. */
    args.uid = BUS_MODEL_UID;
    modelc_parse_arguments(&args, argc, argv, CLI_DOC);
    if (args.timeout <= 0) args.timeout = BUS_TIMEOUT;

    log_notice("Transport:");
    log_notice("  transport: %s", args.transport);
    log_notice("  uri: %s", args.uri);

    if (args.transport && strcmp(args.transport, "loopback") == 0) {
        log_notice("Loopback operation of SimBus not supported.");
        exit(0);
    }

    /* Create Endpoint and Adapter objects. */
    int       retry_count = 60;
    Endpoint* endpoint = NULL;
    log_notice("Create the Endpoint object ...");
    while (--retry_count) {
        endpoint = endpoint_create(
            args.transport, args.uri, args.uid, true, args.timeout);
        if (endpoint) break;
        sleep(1);
        log_info("Retry endpoint creation ...");
    }
    if (endpoint == NULL) {
        log_fatal("Could not create endpoint!");
    }
    log_notice("Create the Bus Adapter object ...");
    Adapter* adapter = simbus_adapter_create(endpoint, args.step_size);
    log_notice("  Model UID: %d", adapter->bus_adapter_model->model_uid);


    /* Configure all specified channels. */
    YamlNode* model_node;
    model_node = dse_yaml_find_node_in_seq_in_doclist(
        args.yaml_doc_list, "Stack", "spec/models", "name", args.name);
    YamlNode* ch_seq_node;
    ch_seq_node = dse_yaml_find_node(model_node, "channels");
    if (ch_seq_node) {
        for (uint32_t i = 0; i < hashlist_length(&ch_seq_node->sequence); i++) {
            YamlNode* ch_node = hashlist_at(&ch_seq_node->sequence, i);
            YamlNode* n_node = dse_yaml_find_node(ch_node, "name");
            YamlNode* emc_node =
                dse_yaml_find_node(ch_node, "expectedModelCount");
            uint32_t _model_count = (emc_node) ? atol(emc_node->scalar) : 0;

            log_notice("  Channel: %s (expected models=%u)", n_node->scalar,
                _model_count);
            adapter_init_channel(
                adapter->bus_adapter_model, n_node->scalar, NULL, 0);
            simbus_adapter_init_channel(
                adapter->bus_adapter_model, n_node->scalar, _model_count);
        }
    } else {
        /* Fallback if missing configuration. */
        log_error("No channel configuration found, fallback ...");
        log_error(
            "  Channel: %s (expected models=%u)", ADAPTER_FALLBACK_CHANNEL, 1);
        adapter_init_channel(
            adapter->bus_adapter_model, ADAPTER_FALLBACK_CHANNEL, NULL, 0);
        simbus_adapter_init_channel(
            adapter->bus_adapter_model, ADAPTER_FALLBACK_CHANNEL, 1);
    }

    log_notice("Start the Bus ...");
    simbus_adapter_run(adapter);
    {
        log_simbus("========================================");
        log_simbus("Adapter Dump");
        log_simbus("========================================");
        adapter_model_dump_debug(adapter->bus_adapter_model, "SimBus");
        log_simbus("----------------------------------------");
        log_simbus("bus_mode       : %u", adapter->bus_mode);
        log_simbus("bus_time       : %f", adapter->bus_time);
        log_simbus("bus_step_size : %f", adapter->bus_step_size);
        log_simbus("========================================");
    }
    adapter_destroy(adapter);

    exit(0);
}
