// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>


#define CSV_LINE_MAXLEN 1024
#define CSV_DELIMITER   ",;"
#define CSV_FILE_ENVAR  "CSV_FILE"


typedef struct {
    ModelDesc model;

    /* CSV specific members. */
    const char*   file_name;
    FILE*         file;
    char          line[CSV_LINE_MAXLEN];
    double        timestamp;
    SignalVector* sv;
} CsvModelDesc;


static bool read_csv_line(CsvModelDesc* c)
{
    c->timestamp = -1;  // Set a safe time (i.e. no valid value set).
    while (c->timestamp < 0) {
        // Read a line.
        char* line = fgets(c->line, CSV_LINE_MAXLEN, c->file);
        if (line == NULL) return false;
        if (strlen(line) == 0) continue;

        // Get the timestamp.
        log_trace("csv::%s", c->line);
        errno = 0;
        double ts = strtod(c->line, NULL);
        if (errno) {
            log_error("Bad line, timestamp conversion failed");
            log_error(c->line);
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

    /* Open and prepare the CSV file (i.e. consume the first line). */
    c->file_name = getenv(CSV_FILE_ENVAR);
    c->file = fopen(c->file_name, "r");
    if (c->file == NULL) {
        log_fatal("Unable to open CSV file (%s)", c->file_name);
    }
    fgets(c->line, CSV_LINE_MAXLEN, c->file);
    log_trace("csv::%s", c->line);

    /* Read the first CSV value set. */
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
                c->sv->scalar[idx] = v;
            }
            if (++idx >= c->sv->count) break; /* Too many values. */
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
}
