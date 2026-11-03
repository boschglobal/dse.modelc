// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package inspect

import (
	"bytes"
	"context"
	"errors"
	"strings"
	"testing"
)

type failingTask struct{}

func (failingTask) Name() string { return "Failing" }

func (failingTask) Function(ctx context.Context, ins *Inspect) TaskResult {
	return TaskResult{Err: errors.New("boom")}
}

func TestRunPrintsTaskError(t *testing.T) {
	var buf bytes.Buffer
	ins := NewInspect("testdata/lua/sim",
		WithTask(ListAllFiles{}),
		WithTask(failingTask{}),
		WithResultPrinter(NewTextPrinter(&buf)),
	)

	err := ins.Run(context.Background())
	if err == nil || !strings.Contains(err.Error(), "Failing: boom") {
		t.Fatalf("Run() error = %v, want it to contain %q", err, "Failing: boom")
	}

	// Later tasks still run and the failure is reported in its own section.
	want := "# Failing\n\n## error\n\n```text\nboom\n```\n"
	if got := buf.String(); !strings.HasSuffix(got, want) {
		t.Errorf("output does not end with error section\ngot:\n%s\nwant suffix:\n%s", got, want)
	}
	if !strings.Contains(buf.String(), "# List Files\n") {
		t.Errorf("output missing results of the successful task:\n%s", buf.String())
	}
	// Results are separated by an empty line.
	if !strings.Contains(buf.String(), "```\n\n# Failing\n") {
		t.Errorf("results are not separated by an empty line:\n%s", buf.String())
	}
}

func TestTextPrinterHardensFence(t *testing.T) {
	var buf bytes.Buffer
	NewTextPrinter(&buf)(TaskResult{
		TargetName: "T",
		Details:    map[string]any{"k": []string{"a ```` b", "``` c"}},
	})

	want := "# T\n\n## k\n\n`````text\na ```` b\n``` c\n`````\n"
	if got := buf.String(); got != want {
		t.Errorf("output mismatch\ngot:\n%s\nwant:\n%s", got, want)
	}
}
