// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package inspect

import (
	"bytes"
	"context"
	"strings"
	"testing"
)

// inventoryOutput runs the Inventory task and returns the printed output.
func inventoryOutput(t *testing.T, simPath string) string {
	t.Helper()
	var buf bytes.Buffer
	ins := NewInspect(simPath, WithTask(Inventory{}), WithResultPrinter(NewTextPrinter(&buf)))
	if err := ins.Run(context.Background()); err != nil {
		t.Fatalf("Run() error = %v", err)
	}
	return buf.String()
}

func TestInventoryLua(t *testing.T) {
	got := inventoryOutput(t, "testdata/lua/sim")

	// Single quotes stand in for backticks.
	want := strings.ReplaceAll(`# Simulation Inventory

## Stack: default

File: 'data/simulation.yaml'

| Parameter | Value |
|---|---|
| name | default |
| stacked | false |
| stepsize | 0.0005 |
| endtime | 0.005 |
| external | false |

| Transport | Value |
|---|---|
| name | redis |
| uri | 'redis://localhost:6379' |
| timeout | 60 |

| Annotation | Value |
|---|---|
| simulation.endtime | 0.005 |
| simulation.stepsize | 0.0005 |

### SimBus

| Parameter | Value |
|---|---|
| name | simbus |
| model | simbus |
| uid | 0 |
| operation | started by simer |

| Channel | Expected Models |
|---|---|
| physical | 2 |

| Files |
|---|
| 'data/simulation.yaml' |

### Model: input (Csv)

| Parameter | Value |
|---|---|
| name | input |
| model | Csv |
| uid | 1 |
| operation | started by simer |
| mcl | lua |

| Channel | Alias | Selector |
|---|---|---|
| physical | signal_channel | channel: signal_vector, model: input |

| Env | Value |
|---|---|
| CSV_FILE | model/input/data/input.csv |

| Files |
|---|
| 'data/simulation.yaml' |
| 'model/input/csv.lua' |
| 'model/input/data/input.csv' |
| 'model/input/data/signalgroup.yaml' |

### Model: linear (Linear)

| Parameter | Value |
|---|---|
| name | linear |
| model | Linear |
| uid | 2 |
| operation | started by simer |
| mcl | lua |

| Channel | Alias | Selector |
|---|---|---|
| physical | signal_channel | channel: signal_vector, model: linear |

| Files |
|---|
| 'data/simulation.yaml' |
| 'model/linear/linear.lua' |
| 'model/linear/data/signalgroup.yaml' |
`, "'", "`")
	if got != want {
		t.Errorf("output mismatch\ngot:\n%s\nwant:\n%s", got, want)
	}
}

func TestInventoryFMI(t *testing.T) {
	got := inventoryOutput(t, "testdata/fmi/sim")

	// Single quotes stand in for backticks.
	want := strings.ReplaceAll(`# Simulation Inventory

## Stack: default

File: 'data/simulation.yaml'

| Parameter | Value |
|---|---|
| name | default |
| stacked | false |
| stepsize | 0.0005 |
| endtime | 0.005 |
| external | false |

| Transport | Value |
|---|---|
| name | redis |
| uri | 'redis://localhost:6379' |
| timeout | 60 |

| Annotation | Value |
|---|---|
| simulation.endtime | 0.005 |
| simulation.stepsize | 0.0005 |

### SimBus

| Parameter | Value |
|---|---|
| name | simbus |
| model | simbus |
| uid | 0 |
| operation | started by simer |

| Channel | Expected Models |
|---|---|
| physical | 2 |

| Files |
|---|
| 'data/simulation.yaml' |

### Model: input (dse.modelc.csv)

| Parameter | Value |
|---|---|
| name | input |
| model | dse.modelc.csv |
| uid | 1 |
| operation | started by simer |

| Channel | Alias | Selector |
|---|---|---|
| physical | signal_channel | channel: signal_vector, model: input |

| Env | Value |
|---|---|
| CSV_FILE | model/input/data/input.csv |
| SIMBUS_LOGLEVEL | 5 |

| Runtime | Value |
|---|---|
| dynlib.linux.amd64 | 'model/input/lib/libcsv.so' |
| dynlib.linux.x86 | 'model/input/lib/libcsv.so' |
| dynlib.linux.i386 | 'model/input/lib/libcsv.so' |
| dynlib.windows.x64 | 'model/input/lib/libcsv.dll' |
| dynlib.windows.x86 | 'model/input/lib/libcsv.dll' |

| Files |
|---|
| 'data/simulation.yaml' |
| 'model/input/data/model.yaml' |
| 'model/input/data/signalgroup.yaml' |

### Model: linear (linear)

| Parameter | Value |
|---|---|
| name | linear |
| model | linear |
| uid | 2 |
| operation | started by simer |
| mcl | fmi 2.0 |

| Channel | Alias | Selector |
|---|---|---|
| physical | signal_channel | channel: signal_vector, model: linear |

| Env | Value |
|---|---|
| MEASUREMENT_FILE | /sim/measurement.mf4 |
| SIMBUS_LOGLEVEL | 2 |

| Runtime | Value |
|---|---|
| dynlib.linux.amd64 | 'model/linear/lib/libfmimcl.so' |
| mcl.linux.amd64 | 'model/linear/linear_fmu/binaries/linux64/fmu2linear.so' |

| Model Annotation | Value |
|---|---|
| fmi_guid | {71da084a-0998-4418-a29d-01af36a32568} |
| fmi_model_cosim | true |
| fmi_model_version |  |
| fmi_resource_dir | model/linear/linear_fmu/resources |
| fmi_stepsize | 0.0005 |
| mcl_adapter | fmi |
| mcl_version | 2.0 |

| Files |
|---|
| 'data/simulation.yaml' |
| 'model/linear/data/model.yaml' |
| 'model/linear/data/signalgroup.yaml' |
`, "'", "`")
	if got != want {
		t.Errorf("output mismatch\ngot:\n%s\nwant:\n%s", got, want)
	}
}

// Gateway has two stacks; the second has no simbus and an external model.
func TestInventoryGateway(t *testing.T) {
	got := inventoryOutput(t, "testdata/gateway/sim")

	// Single quotes stand in for backticks.
	want := strings.ReplaceAll(`# Simulation Inventory

## Stack: external

File: 'data/simulation.yaml'

| Parameter | Value |
|---|---|
| name | external |
| stacked | false |
| stepsize | 0.0005 |
| endtime | 0.005 |
| external | true |

| Transport | Value |
|---|---|
| name | redis |
| uri | 'redis://localhost:6379' |
| timeout | 60 |

| Annotation | Value |
|---|---|
| simulation.endtime | 0.005 |
| simulation.stepsize | 0.0005 |

### Model: gateway (gateway)

| Parameter | Value |
|---|---|
| name | gateway |
| model | gateway |
| uid | 3 |
| operation | not started by simer (external) |

| Channel | Alias | Selector |
|---|---|---|
| signal | data | channel: data, model: gateway |

## Stack: model

File: 'data/simulation.yaml'

| Parameter | Value |
|---|---|
| name | model |
| stacked | false |
| stepsize | 0.0005 |
| endtime | 0.005 |
| external | false |

| Transport | Value |
|---|---|
| name | redis |
| uri | 'redis://localhost:6379' |
| timeout | 60 |

| Annotation | Value |
|---|---|
| simulation.endtime | 0.005 |
| simulation.stepsize | 0.0005 |

### SimBus

| Parameter | Value |
|---|---|
| name | simbus |
| model | simbus |
| uid | 0 |
| operation | started by simer |

| Channel | Expected Models |
|---|---|
| signal | 3 |

| Files |
|---|
| 'data/simulation.yaml' |

### Model: input (dse.modelc.csv)

| Parameter | Value |
|---|---|
| name | input |
| model | dse.modelc.csv |
| uid | 1 |
| operation | started by simer |

| Channel | Alias | Selector |
|---|---|---|
| signal | signal_channel | channel: signal_vector, model: input |

| Env | Value |
|---|---|
| CSV_FILE | model/input/data/input.csv |
| SIMBUS_LOGLEVEL | 5 |

| Runtime | Value |
|---|---|
| dynlib.linux.amd64 | 'model/input/lib/libcsv.so' |
| dynlib.linux.x86 | 'model/input/lib/libcsv.so' |
| dynlib.linux.i386 | 'model/input/lib/libcsv.so' |
| dynlib.windows.x64 | 'model/input/lib/libcsv.dll' |
| dynlib.windows.x86 | 'model/input/lib/libcsv.dll' |

| Files |
|---|
| 'data/simulation.yaml' |
| 'model/input/data/model.yaml' |
| 'model/input/data/signalgroup.yaml' |

### Model: linear (linear)

| Parameter | Value |
|---|---|
| name | linear |
| model | linear |
| uid | 2 |
| operation | started by simer |
| mcl | fmi 2.0 |

| Channel | Alias | Selector |
|---|---|---|
| signal | signal_channel | channel: signal_vector, model: linear |

| Env | Value |
|---|---|
| MEASUREMENT_FILE | /sim/measurement.mf4 |
| SIMBUS_LOGLEVEL | 4 |

| Runtime | Value |
|---|---|
| dynlib.linux.amd64 | 'model/linear/lib/libfmimcl.so' |
| mcl.linux.amd64 | 'model/linear/linear_fmu/binaries/linux64/fmu2linear.so' |

| Model Annotation | Value |
|---|---|
| fmi_guid | {71da084a-0998-4418-a29d-01af36a32568} |
| fmi_model_cosim | true |
| fmi_model_version |  |
| fmi_resource_dir | model/linear/linear_fmu/resources |
| fmi_stepsize | 0.0005 |
| mcl_adapter | fmi |
| mcl_version | 2.0 |

| Files |
|---|
| 'data/simulation.yaml' |
| 'model/linear/data/model.yaml' |
| 'model/linear/data/signalgroup.yaml' |
`, "'", "`")
	if got != want {
		t.Errorf("output mismatch\ngot:\n%s\nwant:\n%s", got, want)
	}
}

func TestMclName(t *testing.T) {
	tests := []struct {
		name string
		ann  map[string]any
		want string
	}{
		{"configured", map[string]any{"mcl_adapter": "fmi", "mcl_version": "2.0"}, "fmi 2.0"},
		{"no version", map[string]any{"mcl_adapter": "fmi"}, "not configured correctly (missing annotation: mcl_version)"},
		{"no adapter", map[string]any{"mcl_version": "2.0"}, "not configured correctly (missing annotation: mcl_adapter)"},
		{"none", nil, "not configured correctly (missing annotation: mcl_adapter, mcl_version)"},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := mclName(tt.ann); got != tt.want {
				t.Errorf("mclName() = %q, want %q", got, tt.want)
			}
		})
	}
}

func TestRenderInstanceAnnotations(t *testing.T) {
	m := InventoryModel{
		Name:             "m",
		Model:            "M",
		Annotations:      map[string]any{"a": "1"},
		ModelAnnotations: map[string]any{"b": map[string]any{"c": "2"}},
	}
	got := strings.Join(renderInstance(m), "\n")
	want := "| Instance Annotation | Value |\n|---|---|\n| a | 1 |\n\n" +
		"| Model Annotation | Value |\n|---|---|\n| b.c | 2 |\n"
	if !strings.Contains(got, want) {
		t.Errorf("annotation tables missing\ngot:\n%s", got)
	}
}
