// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package inspect

import (
	"bytes"
	"context"
	"regexp"
	"testing"
)

// Mod times vary per checkout.
var modTimeRe = regexp.MustCompile(`\d{4}-\d{2}-\d{2}T\d{2}:\d{2}Z`)

func TestListAllFiles(t *testing.T) {
	var buf bytes.Buffer
	ins := NewInspect("testdata/lua/sim", WithTask(ListAllFiles{}), WithResultPrinter(NewTextPrinter(&buf)))

	if err := ins.Run(context.Background()); err != nil {
		t.Fatalf("Run() error = %v", err)
	}

	want := "# List Files\n" +
		"\n## format\n\n```text\n" +
		"type(d|f) size(-=dir) modified(UTC) path\n" +
		"```\n" +
		"\n## manifest\n\n```text\n" +
		"d      - T data/\n" +
		"f   1.2K T data/simulation.yaml\n" +
		"d      - T model/\n" +
		"d      - T model/input/\n" +
		"f   2.8K T model/input/csv.lua\n" +
		"d      - T model/input/data/\n" +
		"f   109B T model/input/data/input.csv\n" +
		"f   187B T model/input/data/signalgroup.yaml\n" +
		"d      - T model/linear/\n" +
		"d      - T model/linear/data/\n" +
		"f   818B T model/linear/data/signalgroup.yaml\n" +
		"f   1.5K T model/linear/linear.lua\n" +
		"d      - T trace/\n" +
		"```\n"
	if got := modTimeRe.ReplaceAllString(buf.String(), "T"); got != want {
		t.Errorf("output mismatch\ngot:\n%s\nwant:\n%s", got, want)
	}
}
