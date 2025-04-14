// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package summary

import (
	"errors"
	"io"
	"os"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"
)

type StdoutCapture struct {
	oldStdout *os.File
	readPipe  *os.File
}

func (sc *StdoutCapture) StartCapture() {
	sc.oldStdout = os.Stdout
	sc.readPipe, os.Stdout, _ = os.Pipe()
}

func (sc *StdoutCapture) StopCapture() (string, error) {
	if sc.oldStdout == nil || sc.readPipe == nil {
		return "", errors.New("StartCapture not called before StopCapture")
	}
	os.Stdout.Close()
	os.Stdout = sc.oldStdout
	bytes, err := io.ReadAll(sc.readPipe)
	if err != nil {
		return "", err
	}
	return string(bytes), nil
}

func TestShortSummary(t *testing.T) {
	capture := StdoutCapture{}
	capture.StartCapture()

	var v trace.Visitor = &Short{}
	trace := trace.Stream{File: "../../../testdata/simbus.bin"}
	err := trace.Process(&v)
	assert.Nil(t, err)

	output, _ := capture.StopCapture()
	t.Log("\n\n", output)
	outputList := strings.Split(output, "\n")

	assert.Contains(t, outputList, "scalar_channel:42:1:0::ModelRegister")
	assert.Contains(t, outputList, "binary_channel:42:2:0::ModelRegister")
	assert.Contains(t, outputList, "scalar_channel:42:0:0::SignalIndex")
	assert.Contains(t, outputList, "binary_channel:42:0:0::SignalValue")
	assert.Contains(t, outputList, "binary_channel:42:0:0::SignalRead")
	assert.Contains(t, outputList, "Notify:0.002000:0.000000:0.002500 (S)-->(M) (0)")
	assert.Contains(t, outputList, "Notify:0.002000:0.000000:0.000000 (S)<--(M) (42)")
	assert.Contains(t, outputList, "scalar_channel:42:0:0::ModelExit")
	assert.Contains(t, outputList, "binary_channel:42:0:0::ModelExit")
}

func TestLongSummary(t *testing.T) {
	capture := StdoutCapture{}
	capture.StartCapture()

	var v trace.Visitor = &Long{SignalLookup: map[uint32]string{}}
	trace := trace.Stream{File: "../../../testdata/simbus.bin"}
	err := trace.Process(&v)
	assert.Nil(t, err)

	output, _ := capture.StopCapture()
	t.Log("\n\n", output)
	outputList := strings.Split(output, "\n")

	assert.Contains(t, outputList, "ModelRegister:42:scalar_channel:1:0")
	assert.Contains(t, outputList, "SignalIndex:42:scalar_channel:0:0")
	assert.Contains(t, outputList, "SignalIndex:42:scalar_channel:counter=2628574755")
	assert.Contains(t, outputList, "SignalValue:42:scalar_channel:0:0")
	assert.Contains(t, outputList, "SignalValue:42:scalar_channel:counter=0.000000")
	assert.Contains(t, outputList, "SignalRead:42:scalar_channel:0:0")
	assert.Contains(t, outputList, "SignalRead:42:scalar_channel:counter")
	assert.Contains(t, outputList, "[0.000000] Notify:0.000000:0.000000:0.000500 (S)-->(M) (0)")
	assert.Contains(t, outputList, "[0.000000] Notify[8000008]:SignalVector:scalar_channel:Signal:counter=42.000000")
	assert.Contains(t, outputList, "[0.000000] Notify:0.000000:0.000000:0.000000 (S)<--(M) (42)")
	assert.Contains(t, outputList, "[0.000000] Notify[42]:SignalVector:scalar_channel:Signal:counter=42.000000")
	assert.Contains(t, outputList, "[0.000500] Notify[8000008]:SignalVector:binary_channel:Signal:message=len(12)")
	assert.Contains(t, outputList, "[0.000500] Notify[42]:SignalVector:binary_channel:Signal:message=len(12)")
	assert.Contains(t, outputList, "ModelExit:42:scalar_channel:0:0")
}
