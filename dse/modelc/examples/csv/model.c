// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/clib/csv/csv.h>
#include <dse/clib/collections/vector.h>
#include <dse/modelc/model.h>


typedef struct {
    ModelDesc model;

    /* CSV specific members. */
    CsvDesc       csv;
    double        timestamp;
    SignalVector* sv;
    Vector        index; /* vector_at[idx] -> *double */
} CsvModelDesc;


static bool read_csv_line(CsvModelDesc* c)
{
    c->timestamp = -1;  // Set a safe time (i.e. no valid value set).
    while (c->timestamp < 0) {
        int rc = csv_next(&c->csv);
        if (rc == -ENODATA) return false;
        if (rc != 0) {
            log_error("Bad line, csv_next failed (%d)", rc);
            continue;
        }

        double ts = csv_field(&c->csv, 0);
        if (isnan(ts)) continue;
        if (ts >= 0) c->timestamp = ts;
    }

    return true;
}


ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    CsvModelDesc* c = calloc(1, sizeof(CsvModelDesc));
    memcpy(c, model, sizeof(ModelDesc));
    if (model->sv && model->sv->name) {
        c->sv = model->sv;
    } else {
        log_fatal("No signal vector configured");
    }

    /* Open and prepare the CSV file. */
    c->csv = csv_open(NULL);
    if (c->csv.file == NULL) {
        log_fatal("Unable to open CSV file (%s)", c->csv.file_name);
    }

    /* Build an index. */
    c->index = vector_make(sizeof(double*), 0, NULL);
    size_t      col = 1;  // Skip timestamp at column 0.
    const char* signal_name = NULL;
    while ((signal_name = csv_header(&c->csv, col++))) {
        for (size_t i = 0; i < c->sv->count; i++) {
            if (strcmp(signal_name, c->sv->signal[i]) == 0) {
                double* signal_val = &c->sv->scalar[i];
                vector_push(&c->index, &signal_val);
            }
        }
    }

    /* Read the first line (preload). */
    read_csv_line(c);

    /* Return the extended object. */
    return (ModelDesc*)c;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    CsvModelDesc* c = (CsvModelDesc*)model;

    /* Apply value sets from CSV. */
    while ((c->timestamp >= 0) && (c->timestamp <= *model_time)) {
        size_t idx = 0;
        size_t count = csv_count(&c->csv);
        for (size_t col = 1; col < count; col++) {
            double v = csv_field(&c->csv, col);
            if (isnan(v)) continue;

            double* signal = NULL;
            vector_at(&c->index, idx, &signal);
            if (signal) *signal = v;

            /* Check limit. */
            if (++idx >= vector_len(&c->index)) break;
        }
        /* Load the next line. */
        if (read_csv_line(c) == false) break;
    }

    /* Advance the model time. */
    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    CsvModelDesc* c = (CsvModelDesc*)model;
    csv_close(&c->csv);
    vector_reset(&c->index);
}
