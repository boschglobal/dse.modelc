// Copyright 2023 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_MODELC_GATEWAY_H_
#define DSE_MODELC_GATEWAY_H_

#include <stdint.h>
#include <dse/platform.h>
#include <dse/modelc/model.h>


/**
 *  Dynamic Simulation Environment - Model C - Gateway
 *  ==================================================
 *
 *  The Model C Gateway allows a foreign model to connect to, and exchange
 *  signals with, a DSE based Simulation (i.e. SimBus).
 *
 *  Example
 *  -------
 *  #include <stddef.h>
 *  #include <dse/modelc/gateway.h>
 *
 *  extern uint8_t __log_level__;
 *
 *  void main(void)
 *  {
 *      double model_time = 0.0;
 *      double step_size = 0.05;
 *      double end_time = 0.2;
 *
 *      ModelGatewayDesc gw;
 *      SignalVector*    sv;
 *
 *      // Setup the gateway.
 *      double      gw_foo = 0.0;
 *      double      gw_bar = 42.0;
 *      const char* yaml_files[] = {
 *          "resources/model/gateway.yaml",
 *          NULL,
 *      };
 *      model_gw_setup(
 *          &gw, "gateway", yaml_files, __log_level__, step_size, end_time);
 *
 *      // Find the scalar signal vector.
 *      sv = gw.sv;
 *      while (sv && sv->name) {
 *          if (strcmp(sv->name, "scalar") == 0) break;
 *          // Next signal vector.
 *          sv++;
 *      }
 *
 *      // Run the simulation.
 *      while (model_time < end_time) {
 *          // Marshal data _to_ the signal vector.
 *          sv->scalar[0] = gw_foo;
 *          sv->scalar[1] = gw_bar;
 *          // Synchronise the gateway.
 *          model_gw_sync(&gw, model_time)
 *          // Marshal data _from_ the signal vector.
 *          gw_foo = sv->scalar[0];
 *          gw_bar = sv->scalar[1];
 *          // Model function.
 *          gw_foo += 1;
 *          gw_bar += gw_foo;
 *          // Next step.
 *          model_time += step_size;
 *      }
 *
 *      // Exit the simulation.
 *      model_gw_exit(&gw);
 *  }
 */


typedef struct ModelGatewayDesc {
    SimulationSpec*    sim;
    ModelInstanceSpec* mi;
    SignalVector*      sv; /* Null terminated list. */
    /* References to allocated memory. */
    const char**       argv;
    char*              name_arg;
} ModelGatewayDesc;


/* gateway.c - Gateway Model Interface. */
DLL_PUBLIC int model_gw_setup(ModelGatewayDesc* gw, const char* name,
    const char** yaml_files, int log_level, double step_size, double end_time);
DLL_PUBLIC int model_gw_sync(ModelGatewayDesc* gw, double model_time);
DLL_PUBLIC int model_gw_exit(ModelGatewayDesc* gw);


#endif  // DSE_MODELC_GATEWAY_H_
