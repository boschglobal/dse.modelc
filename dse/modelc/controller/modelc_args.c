// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/strings.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/runtime.h>
#include <dse/modelc/adapter/transport/endpoint.h>


#define OPT_LIST             "ht:U:H:P:s:X:e:u:n:T:l:f:p:"
#define REDIS_HOST           "localhost"
#define REDIS_PORT           6379
#define TRANSPORT            TRANSPORT_REDISPUBSUB
#define ENV_SIMBUS_TRANSPORT "SIMBUS_TRANSPORT"
#define ENV_SIMBUS_URI       "SIMBUS_URI"
#define ENV_SIMBUS_LOGLEVEL  "SIMBUS_LOGLEVEL"
#define ENV_SIMBUS_TIMEOUT   "SIMBUS_TIMEOUT"


/* CLI related defaults. */
#define MODEL_UID            0
#define MODEL_TIMEOUT        60
#define TRANSPORT            TRANSPORT_REDISPUBSUB


static struct option long_options[] = {
    { "help", no_argument, NULL, 'h' },
    { "transport", required_argument, NULL, 't' },
    { "uri", required_argument, NULL, 'U' },
    { "host", required_argument, NULL, 'H' },
    { "port", required_argument, NULL, 'P' },
    { "stepsize", required_argument, NULL, 's' },
    { "steps", required_argument, NULL, 'X' },
    { "endtime", required_argument, NULL, 'e' },
    { "uid", required_argument, NULL, 'u' },
    { "name", required_argument, NULL, 'n' },
    { "timeout", required_argument, NULL, 'T' },
    { "logger", required_argument, NULL, 'l' },
    { "file", required_argument, NULL, 'f' },
    { "path", required_argument, NULL, 'p' },
    { 0, 0, 0, 0 },
};


static void print_usage(const char* doc_string)
{
    log_notice(doc_string);
    log_notice("usage: [--transport <transport>]");
    log_notice("       [--uri <endpoint>]  (i.e. redis://localhost:6379)");
    log_notice("       [--host <host url>] *** depreciated, use --uri ***");
    log_notice("       [--port <port number>] *** depreciated, use --uri ***");
    log_notice("       [--stepsize <double>]");
    log_notice("       [--endtime <double>]");
    log_notice("       [--uid <model uid>]");
    log_notice("       [--name <model name>] *** normally required ***");
    log_notice("       [--timeout <double>]");
    log_notice(
        "       [--logger <number>] 0..6 *** 0=more, 6=less, 3=INFO ***");
    log_notice("       [--file <model file>]");
    log_notice("       [--path <path to model>] *** relative path to Model "
               "Package ***");
    log_notice("       [YAML FILE [,YAML FILE] ...]");
}


/*
 *  _args_extract_environment
 *
 *  Extract arguments from environment variables.
 *      SIMBUS_TRANSPORT = "redispubsub" | "mq"  (see endpoint.h)
 *      SIMBUS_URI = "redis://localhost:6379"
 *      SIMBUS_LOGLEVEL = 0 .. 6
 *
 *  Arguments are only modifed if they are not already set by CLI.
 *
 *  Parameters
 *  ----------
 *  args :  ModelCArguments*
 *      Pointer to the arguments structure being filled/completed.
 *
 *  Returns
 *  -------
 *      void
 */
static void _args_extract_environment(ModelCArguments* args)
{
    if (args->transport == NULL) {
        char* _env = getenv(ENV_SIMBUS_TRANSPORT);
        if (_env) {
            static char transport[MAX_URI_LEN];
            strncpy(transport, _env, MAX_URI_LEN - 1);
            args->transport = transport;
        }
    }
    if (args->uri == NULL) {
        char* _env = getenv(ENV_SIMBUS_URI);
        if (_env) {
            static char uri[MAX_URI_LEN];
            strncpy(uri, _env, MAX_URI_LEN - 1);
            args->uri = uri;
        }
    }
    if (args->log_level_set_by_cli == 0) {
        char* _env = getenv(ENV_SIMBUS_LOGLEVEL);
        args->log_level = (_env) ? atol(_env) : args->log_level;
    }
    if (args->timeout_set_by_cli == 0) {
        char* _env = getenv(ENV_SIMBUS_TIMEOUT);
        args->timeout = (_env) ? atof(_env) : args->timeout;
    }
}


#define MODEL_NAME_SEP ";,"

void* modelc_find_stack(ModelCArguments* args)
{
    /* Locate a model instance name (the first will do). */
    if (args->name == NULL) return NULL;
    char* name = strdup(args->name);
    char* lasts = NULL;
    char* model_name = strtok_r(name, MODEL_NAME_SEP, &lasts);
    if (model_name == NULL) {
        free(name);
        return NULL;
    }
    log_debug("Search for stack containing model instance: %s", model_name);

    /* Locate the stack containing the model instance. */
    YamlNode* doc;
    YamlNode* node;
    for (uint32_t i = 0; i < hashlist_length(args->yaml_doc_list); i++) {
        doc = hashlist_at(args->yaml_doc_list, i);

        /* kind is "Stack". */
        node = dse_yaml_find_node(doc, "kind");
        if (node == NULL || node->scalar == NULL) continue;
        if (strcmp(node->scalar, "Stack") != 0) continue;

        /* Stack contains one of the provided model instances. */
        const char* s[] = { "name" };
        const char* v[] = { model_name };
        node = dse_yaml_find_node_in_seq(doc, "spec/models", s, v, 1);
        if (node != NULL) {
            free(name);
            return doc;
        }
    }

    free(name);
    return NULL;
}


static void _args_extract_yaml_connection(ModelCArguments* args)
{
    YamlNode* stack = modelc_find_stack(args);
    if (stack == NULL) {
        log_fatal("No stack found!");
    }
    YamlNode* c_node = dse_yaml_find_node(stack, "spec/connection");
    YamlNode* t_node = dse_yaml_find_node(stack, "spec/connection/transport");

    /* Look for Model Timeout at spec/connection/timeout. */
    if (c_node) {
        if (args->timeout_set_by_cli == 0) {
            YamlNode* node = dse_yaml_find_node(c_node, "timeout");
            args->timeout = (node) ? atof(node->scalar) : args->timeout;
        }
    }

    /* Look for Transport. */
    if (t_node) {
        YamlNode* e_node = NULL;
        if (args->transport) {
            e_node = dse_yaml_find_node(t_node, args->transport);
        } else {
            // take the first child node ...
            if (hashmap_number_keys(t_node->mapping)) {
                char**   keys = hashmap_keys(&t_node->mapping);
                uint32_t count = hashmap_number_keys(t_node->mapping);
                e_node = hashmap_get(&t_node->mapping, keys[0]);
                for (uint32_t _ = 0; _ < count; _++)
                    free(keys[_]);
                free(keys);
            }
        }
        if (e_node) {
            YamlNode* node;
            if (args->transport == NULL) {
                args->transport = e_node->name;
            }
            if (args->uri == NULL) {
                node = dse_yaml_find_node(e_node, "uri");
                args->uri = (node) ? node->scalar : NULL;
            }
            if (args->host == NULL) {
                node = dse_yaml_find_node(e_node, "host");
                args->host = (node) ? node->scalar : NULL;
            }
            if (args->port == 0) {
                node = dse_yaml_find_node(e_node, "port");
                args->port = (node) ? atol(node->scalar) : 0;
            }
        }
    }
}


void modelc_set_default_args(ModelCArguments* args,
    const char* model_instance_name, double step_size, double end_time)
{
    *args = (ModelCArguments){};
    args->timeout = MODEL_TIMEOUT;
    args->log_level = LOG_NOTICE;
    args->step_size = step_size;
    args->end_time = end_time;
    args->uid = MODEL_UID;
    args->name = model_instance_name;
}


void modelc_parse_arguments(
    ModelCArguments* args, int argc, char** argv, const char* doc_string)
{
    extern int   optind, optopt;
    extern char* optarg;
    int          c;

    /* Help specified? */
    optind = 1;  // Reset (to 1) because tests repeatedly call this function.
    while ((c = getopt_long(argc, argv, OPT_LIST, long_options, NULL)) != -1) {
        switch (c) {
        case 'h':
            print_usage(doc_string);
            exit(1);
        default:
            break;
        }
    }
    /* Parse the named options. */
    optind = 1;
    while ((c = getopt_long(argc, argv, OPT_LIST, long_options, NULL)) != -1) {
        switch (c) {
        case 'h':
            break;
        case 't':
            args->transport = optarg;
            break;
        case 'U':
            args->uri = optarg;
            break;
        case 'H':
            args->host = optarg;
            break;
        case 'P':
            args->port = atol(optarg);
            break;
        case 's':
            args->step_size = atof(optarg);
            break;
        case 'e':
            args->end_time = atof(optarg);
            break;
        case 'u':
            args->uid = atol(optarg);
            break;
        case 'n':
            args->name = optarg;
            break;
        case 'T': {
            double _cli_timeout = atof(optarg);
            if (_cli_timeout > 0) {
                args->timeout = atof(optarg);
                args->timeout_set_by_cli = 1;
            }
            break;
        }
        case 'l':
            args->log_level = atol(optarg);
            args->log_level_set_by_cli = 1;
            __log_level__ = args->log_level;
            break;
        case 'f':
            args->file = optarg;
            break;
        case 'p':
            args->path = optarg;
            break;
        case 'X':
            args->steps = atol(optarg);
            break;
        default:
            log_error("unexpected option");
            print_usage(doc_string);
            exit(1);
        }
    }
    /* And any YAML files. */
    while (optind < argc) {
        const char* _file = argv[optind++];
        // FIXME an empty string will circumvent the getopt_long seg.
        if (strlen(_file) == 0) continue;

        char* y_file = dse_path_cat(args->sim_path, _file);
        log_notice("Load YAML File: %s", y_file);
        args->yaml_doc_list = dse_yaml_load_file(y_file, args->yaml_doc_list);
        free(y_file);
    }

    /**
     *  Resolve connection data
     *
     *  Processing is as follows:
     *      CLI specified values are taken first
     *      Fallback to Environment
     *      Fallback to stack
     *      Fallback to default (transport=redispubsub,
     * uri=redis://localhost:6379).
     *
     *  Connections are currently defined by a TRANSPORT and a URI. Its likely
     *  that the TRANSPORT will depreciated (transport will be encoded/detected
     *  via the URI.)
     */
    /* Environment (only if not set). */
    _args_extract_environment(args);
    /* Stack YAML (only if not set). */
    _args_extract_yaml_connection(args);
    /* Default (finally, if not set). */
    if (args->transport == NULL) args->transport = TRANSPORT;
    if (args->host == NULL) args->host = REDIS_HOST;
    if (args->port == 0) args->port = REDIS_PORT;

    /* Construct a URI if provided with legacy Redis args. */
    if (args->uri == NULL) {
        static char uri[MAX_URI_LEN];
        snprintf(uri, MAX_URI_LEN, "redis://%s:%d", args->host, args->port);
        args->uri = uri;
    }

    /* Set the logger level. */
    __log_level__ = args->log_level;
}
