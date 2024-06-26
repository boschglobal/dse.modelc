// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dse/modelc/gateway.h>
#include <dse/logger.h>


/**
 *  Gateway Model.
 *
 *  A normal gateway would exchange signals with the host simulation, however
 *  in this example a simple conversion is applied to each signal (to
 *  demonstrate the effect).
 *
 *  Example
 *  -------
 *      ./gateway 0.005 0.02 gateway.yaml
 */
int main(int argc, char** argv)
{
    if (argc < 4) {
        log_fatal(
            "Missing arguments! (gateway <step_size> <end_time> [yaml ...])");
    }

    double       model_time = 0.0;
    double       step_size = atof(argv[1]);
    double       end_time = atof(argv[2]);
    const char** yaml_files = calloc(argc - 3 + 1, sizeof(char*));
    memcpy(yaml_files, &argv[3], ((argc - 3) * sizeof(char*)));

    /* Setup the gateway. */
    ModelGatewayDesc gw;
    model_gw_setup(&gw, "gateway", yaml_files, LOG_INFO, step_size, end_time);

    /* Run the simulation. */
    while (model_time <= end_time) {
        int rc = model_gw_sync(&gw, model_time);
        if (rc == ETIME) {
            log_notice(
                "Gateway is behind simulation time, advancing gateway time.");
            model_time += step_size;
            continue;
        }

        /* Process the signal vectors. */
        SignalVector* sv = gw.sv;
        while (sv && sv->name) {
            if (sv->is_binary) {
                /* Binary vector. */
                for (uint32_t i = 0; i < sv->count; i++) {
                    log_info("[%f] %s[%d] = <%d:%d>%s (%s)", model_time,
                        sv->name, i, sv->length[i], sv->buffer_size[i],
                        sv->binary[i], sv->signal[i]);
                    /* Exchange/update the gateway signal. */
                    char buffer[100];
                    snprintf(buffer, sizeof(buffer), "st=%f,index=%d",
                        model_time, i);
                    signal_reset(sv, i);
                    signal_append(sv, i, buffer, strlen(buffer) + 1);
                }
            } else {
                /* Scalar vector. */
                for (uint32_t i = 0; i < sv->count; i++) {
                    log_info("[%f] %s[%d] = %f (%s)", model_time, sv->name, i,
                        sv->scalar[i], sv->signal[i]);
                    /* Exchange/update the gateway signal. */
                    sv->scalar[i] = sv->scalar[i] + ((i + 1) << 2);
                }
            }
            /* Next signal vector. */
            sv++;
        }

        /* Next step. */
        model_time += step_size;
    }

    /* Exit the simulation. */
    model_gw_exit(&gw);
    free(yaml_files);

    return 0;
}
