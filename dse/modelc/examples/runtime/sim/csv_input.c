// Copyright 2026 Robert Bosch GmbH
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


typedef struct CsvInputModel {
    ModelDesc     model;
    CsvDesc       csv;
    double        timestamp;
    SignalVector* sv;
    Vector        index;
} CsvInputModel;


static bool read_csv_line(CsvInputModel* model)
{
    model->timestamp = -1;
    while (model->timestamp < 0) {
        int rc = csv_next(&model->csv);
        if (rc == -ENODATA) return false;
        if (rc != 0) continue;

        double timestamp = csv_field(&model->csv, 0);
        if (!isnan(timestamp) && timestamp >= 0) model->timestamp = timestamp;
    }
    return true;
}


ModelDesc* model_create(ModelDesc* model)
{
    CsvInputModel* input = calloc(1, sizeof(CsvInputModel));
    memcpy(input, model, sizeof(ModelDesc));
    if (model->sv == NULL || model->sv->name == NULL)
        log_fatal("No CSV input signal vector configured");
    input->sv = model->sv;
    input->csv = csv_open(NULL);
    if (input->csv.file == NULL) log_fatal("Unable to open CSV input file");

    input->index = vector_make(sizeof(double*), 0, NULL);
    size_t      column = 1;
    const char* name;
    while ((name = csv_header(&input->csv, column++)) != NULL) {
        for (size_t i = 0; i < input->sv->count; i++) {
            if (strcmp(name, input->sv->signal[i]) == 0) {
                double* value = &input->sv->scalar[i];
                vector_push(&input->index, &value);
            }
        }
    }
    read_csv_line(input);
    return (ModelDesc*)input;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    CsvInputModel* input = (CsvInputModel*)model;
    while (input->timestamp >= 0 && input->timestamp <= *model_time) {
        size_t index = 0;
        for (size_t column = 1; column < csv_count(&input->csv); column++) {
            double value = csv_field(&input->csv, column);
            if (!isnan(value)) {
                double* signal = NULL;
                vector_at(&input->index, index, &signal);
                if (signal) *signal = value;
            }
            if (++index >= vector_len(&input->index)) break;
        }
        if (!read_csv_line(input)) break;
    }
    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    CsvInputModel* input = (CsvInputModel*)model;
    csv_close(&input->csv);
    vector_reset(&input->index);
}
