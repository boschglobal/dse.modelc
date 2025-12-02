// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/clib/collections/vector.h>
#include <dse/modelc/model.h>


#define CSV_LINE_MAXLEN       1024
#define CSV_DELIMITER         ",;\n"
#define CSV_FILE_ENVAR        "CSV_FILE"
#define CSV_LINE_MAXLEN_ENVAR "CSV_LINE_MAXLEN"


typedef struct {
    ModelDesc model;

    /* CSV specific members. */
    const char*   file_name;
    FILE*         file;
    char*         line;
    size_t        line_maxlen;
    double        timestamp;
    SignalVector* sv;
    Vector        index; /* vector_at[idx] -> *double */
} CsvModelDesc;


static bool read_csv_line(CsvModelDesc* c)
{
    c->timestamp = -1;  // Set a safe time (i.e. no valid value set).
    while (c->timestamp < 0) {
        // Read a line.
        char* line = fgets(c->line, c->line_maxlen, c->file);
        if (line == NULL) return false;
        if (strlen(line) == 0) continue;

        // Get the timestamp.
        log_trace("csv::%s", c->line);
        errno = 0;
        double ts = strtod(c->line, NULL);
        if (errno) {
            log_error("Bad line, timestamp conversion failed");
            log_error("%s", c->line);
            continue;
        }
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

    /* Allocate the line buffer. */
    c->line_maxlen = CSV_LINE_MAXLEN;
    char* line_len = getenv(CSV_LINE_MAXLEN_ENVAR);
    if (line_len && atoi(line_len)) {
        c->line_maxlen = atoi(line_len);
    }
    c->line = calloc(c->line_maxlen, sizeof(char));

    /* Open and prepare the CSV file. */
    c->file_name = getenv(CSV_FILE_ENVAR);
    c->file = fopen(c->file_name, "r");
    if (c->file == NULL) {
        log_fatal("Unable to open CSV file (%s)", c->file_name);
    }

    /* Build an index. */
    fgets(c->line, CSV_LINE_MAXLEN, c->file);
    log_trace("csv::%s", c->line);
    c->index = vector_make(sizeof(double*), 0, NULL);
    char* _saveptr = NULL;
    strtok_r(c->line, CSV_DELIMITER, &_saveptr);
    char* signal_name;
    while ((signal_name = strtok_r(NULL, CSV_DELIMITER, &_saveptr))) {
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
        char*  _saveptr = NULL;
        char*  _valueptr;
        strtok_r(c->line, CSV_DELIMITER, &_saveptr);  // Consume the timestamp.
        while ((_valueptr = strtok_r(NULL, CSV_DELIMITER, &_saveptr))) {
            errno = 0;
            double v = strtod(_valueptr, NULL);
            if (errno == 0) {
                double* signal = NULL;
                vector_at(&c->index, idx, &signal);
                if (signal) *signal = v;
            } else {
                log_error("Error decoding sample [%f][%u]", c->timestamp, idx);
            }
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
    if (c->file) fclose(c->file);
    free(c->line);
    vector_reset(&c->index);
}
