// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/modelc/runtime.h>
//#include <dse/modelc/mcl.h>
#include <dse/platform.h>
#include <dse/logger.h>


/* Model defaults. */
#define MODEL_STEP_SIZE    0.005
#define MODEL_END_TIME     0.020

/* CLI Parameters. */
#define CLI_DOC            "ModelC - model runner"

/* Other things. */
#define GENERAL_BUFFER_LEN 255

/* Signal handling. */
static SimulationSpec* __sim = NULL;

static void signal_handler(int signum)
{
    fprintf(stdout, "Caught signal (%d)\n", signum);
    switch (signum) {
    case SIGTERM:
    case SIGINT:
#ifdef _WIN32
    case SIGBREAK:
#endif
        errno = ECANCELED;
        break;
    default:
        errno = EINVAL;
        perror("SIGNAL: caught unexpected signal!");
        return;
    }
    /* Attempt a clean exit from the SimBus. */
    perror("Signal caught, starting controlled exit!");
    modelc_shutdown(__sim);
}


/* Example entry point for statically linked Model. */
int main(int argc, char** argv)
{
    int             rc;
    char            general_buffer[GENERAL_BUFFER_LEN];
    ModelCArguments args = {0};
    SimulationSpec  sim  = {0};

    /* Additional bookkeeping data. */
    log_notice("Version: %s", MODELC_VERSION);
    log_notice("Platform: %s-%s", PLATFORM_OS, PLATFORM_ARCH);
    time_t    rawtime;
    struct tm t_info;
    time(&rawtime);
#ifdef _WIN32
    gmtime_s(&t_info, &rawtime);
#else
    gmtime_r(&rawtime, &t_info);
#endif
    log_notice("Time: %.4d-%-.2d-%.2d %.2d:%.2d:%.2d (UTC)",
        1900 + t_info.tm_year, t_info.tm_mon, t_info.tm_mday, t_info.tm_hour,
        t_info.tm_min, t_info.tm_sec);
#ifndef _WIN32
    gethostname(general_buffer, GENERAL_BUFFER_LEN - 1);
    log_notice("Host: %s", general_buffer);
#endif
    getcwd(general_buffer, GENERAL_BUFFER_LEN - 1);
    log_notice("CWD: %s", general_buffer);

    /* Set default arguments and then parse from CLI. */
    modelc_set_default_args(&args, NULL, MODEL_STEP_SIZE, MODEL_END_TIME);
    modelc_parse_arguments(&args, argc, argv, CLI_DOC);
    if (args.name == NULL) {
        log_fatal("name parameter not provided (Model Instance Name)!");
        exit(0);
    }

    /* Configure the Model (takes config from parsed YAML docs). */
    rc = modelc_configure(&args, &sim);
    if (rc) exit(rc);

    /* Start and run the Controller and (dynamically linked) Model. */
    __sim = &sim;
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
#ifdef _WIN32
    signal(SIGBREAK, signal_handler);
#endif
    rc = modelc_run(&sim, false);

    /* Exit the simulation. */
    modelc_exit(&sim);
    exit(rc);
}
