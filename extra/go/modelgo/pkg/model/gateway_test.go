// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package model

import (
	"maps"
	"slices"
	"testing"

	flatbuffers "github.com/google/flatbuffers/go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/connection"
	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/errors"
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/notify"
)

func TestConfiguration(t *testing.T) {
	tests := []struct {
		gw  Gateway
		err error
	}{
		{
			gw:  Gateway{},
			err: errors.ErrGatewayConfig("gateway name not configured"),
		},
		{
			gw:  Gateway{Name: "foo"},
			err: errors.ErrGatewayConfig("gateway uid not configured"),
		},
		{
			gw:  Gateway{Uid: 42},
			err: errors.ErrGatewayConfig("gateway name not configured"),
		},
		{
			gw:  Gateway{Name: "foo", Uid: 42},
			err: errors.ErrGatewayConfig("gateway signal vectors not configured"),
		},
		{
			gw:  Gateway{Name: "foo", Uid: 42, ScalarSignalVector: map[string]*SignalVector[float64]{"foo": {}}},
			err: errors.ErrModelNoConnection,
		},
		{
			gw:  Gateway{Name: "foo", Uid: 42, BinarySignalVector: map[string]*SignalVector[[]byte]{"bar": {}}},
			err: errors.ErrModelNoConnection,
		},
	}
	for _, test := range tests {
		err := test.gw.Connect()
		assert.Equal(t, test.err, err)
	}
}

func checkTraceNotifyMessage(t *testing.T, stub *connection.StubConnection, index uint, channelName string) {
	_ = channelName
	t.Helper()
	msg := stub.Trace[index].Msg
	assert.True(t, flatbuffers.BufferHasIdentifier(msg[4:], notifyMessageIdentifier),
		"trace[%d] expected SBNO identifier", index)
}

func TestConnect(t *testing.T) {
	names := []string{"one", "two", "three"}
	uids := []uint32{123, 456, 789}
	bnames := []string{"four"}
	buids := []uint32{321}
	values := []float64{1.1, 22.22, 333.333}

	// Prime the stub connection with messages.
	stub := &connection.StubConnection{}
	_gw := NewGateway("foo", 42, stub)
	_gw.ScalarSignalVector["scalar"] = NewSignalVector[float64]("scalar").Add(names)
	_gw.BinarySignalVector["binary"] = NewSignalVector[[]byte]("binary").Add(bnames)
	_gw.ScalarSignalVector["scalar"].indexSignals(names, uids)
	_gw.ScalarSignalVector["scalar"].SetByName(names, values)

	// ModelRegister ACKs (one per channel).
	pushRegisterAck(_gw, stub, "scalar", 1)
	pushRegisterAck(_gw, stub, "binary", 2)
	// SignalIndex ACKs (one per channel).
	pushSignalIndexAck(_gw, stub, "scalar", names, uids)
	pushSignalIndexAck(_gw, stub, "binary", bnames, buids)

	require.Len(t, stub.Stack, 4)
	require.Len(t, stub.Trace, 0)

	// Start the test with a new GW object, stub is now primed.
	gw := NewGateway("foo", 42, stub)
	gw.ScalarSignalVector["scalar"] = NewSignalVector[float64]("scalar").Add(names)
	gw.BinarySignalVector["binary"] = NewSignalVector[[]byte]("binary").Add(bnames)
	gw.RegisterRetry = 5
	err := gw.Connect()
	assert.Nil(t, err)

	// Check the UIDs were indexed from the SignalIndex ACK.
	assert.ElementsMatch(t, uids, slices.Collect(maps.Keys(gw.ScalarSignalVector["scalar"].Index.Uid)))
	assert.ElementsMatch(t, buids, slices.Collect(maps.Keys(gw.BinarySignalVector["binary"].Index.Uid)))
	for i := range len(uids) {
		uidIdx := gw.ScalarSignalVector["scalar"].Index.Uid[uids[i]]
		nameIdx := gw.ScalarSignalVector["scalar"].Index.Name[names[i]]
		assert.Equal(t, uidIdx, nameIdx)
	}
	for i := range len(buids) {
		uidIdx := gw.BinarySignalVector["binary"].Index.Uid[buids[i]]
		nameIdx := gw.BinarySignalVector["binary"].Index.Name[bnames[i]]
		assert.Equal(t, uidIdx, nameIdx)
	}

	// Check Trace:
	require.Len(t, stub.Stack, 0)
	require.Len(t, stub.Trace, 8)
	// Sent messages are at indices 0,1 (register) and 4,5 (signalindex).
	checkTraceNotifyMessage(t, stub, 0, "scalar") // sent ModelRegister
	checkTraceNotifyMessage(t, stub, 1, "binary") // sent ModelRegister
	checkTraceNotifyMessage(t, stub, 4, "scalar") // sent SignalIndex
	checkTraceNotifyMessage(t, stub, 5, "binary") // sent SignalIndex

	// Verify trace[0] has ModelRegister embedded.
	{
		msg := stub.Trace[0].Msg
		nMsg := notify.GetSizePrefixedRootAsNotifyMessage(msg, 0)
		mr := nMsg.ModelRegister(nil)
		assert.NotNil(t, mr, "trace[0] should have model_register")
	}
	// Verify trace[4] has SignalIndex embedded.
	{
		msg := stub.Trace[4].Msg
		nMsg := notify.GetSizePrefixedRootAsNotifyMessage(msg, 0)
		si := nMsg.SignalIndex(nil)
		assert.NotNil(t, si, "trace[4] should have signal_index")
	}
}

func TestSync(t *testing.T) {
	names := []string{"one", "two", "three"}
	uids := []uint32{123, 456, 789}
	bnames := []string{"four"}
	buids := []uint32{321}

	stepSize := 0.0005
	stepTotal := 4

	val := func(index int, seed float64) float64 {
		return seed*float64(index) + seed
	}

	// Prime the stub connection with messages.
	stub := &connection.StubConnection{}
	gw := NewGateway("foo", 42, stub)
	gw.ScalarSignalVector["scalar"] = NewSignalVector[float64]("scalar").Add(names)
	gw.BinarySignalVector["binary"] = NewSignalVector[[]byte]("binary").Add(bnames)
	gw.ScalarSignalVector["scalar"].indexSignals(names, uids)
	gw.BinarySignalVector["binary"].indexSignals(bnames, buids)
	for i := range stepTotal {
		gw.ScalarSignalVector["scalar"].SetByName(names, []float64{val(i, 1), val(i, 2), val(i, 3)})
		pushNotify(gw, stub, float64(i+1)*stepSize, float64(i+2)*stepSize)
	}

	// Start the test with a new GW object, stub is now primed.
	gw = NewGateway("foo", 42, stub)
	gw.ScalarSignalVector["scalar"] = NewSignalVector[float64]("scalar").Add(names)
	gw.BinarySignalVector["binary"] = NewSignalVector[[]byte]("binary").Add(bnames)
	gw.ScalarSignalVector["scalar"].indexSignals(names, uids)
	gw.BinarySignalVector["binary"].indexSignals(bnames, buids)
	for step := range stepTotal {
		err := gw.Sync(float64(step) * stepSize)
		require.Nil(t, err)
		assert.Equal(t, float64(step)*stepSize, gw.gatewayTime)
		assert.Equal(t, float64(step+1)*stepSize, gw.modelTime)
		assert.Equal(t, float64(step+2)*stepSize, gw.scheduleTime)
		for i := range len(uids) {
			uidIdx := gw.ScalarSignalVector["scalar"].Index.Uid[uids[i]]
			assert.Equal(t, val(step, float64(i+1)), gw.ScalarSignalVector["scalar"].Signal[uidIdx].Value)
		}
	}

	// Check the Trace.
	require.Len(t, stub.Trace, 8)
}

func TestExit(t *testing.T) {
	names := []string{"one", "two", "three"}
	bnames := []string{"four"}

	stub := &connection.StubConnection{}
	gw := NewGateway("foo", 42, stub)
	gw.ScalarSignalVector["scalar"] = NewSignalVector[float64]("scalar").Add(names)
	gw.BinarySignalVector["binary"] = NewSignalVector[[]byte]("binary").Add(bnames)
	gw.Disconnect()

	// Check the Trace — ModelExit for scalar and binary.
	require.Len(t, stub.Trace, 2)
	checkTraceNotifyMessage(t, stub, 0, "scalar")
	checkTraceNotifyMessage(t, stub, 1, "binary")

	// Verify model_exit is embedded.
	for _, idx := range []uint{0, 1} {
		msg := stub.Trace[idx].Msg
		nMsg := notify.GetSizePrefixedRootAsNotifyMessage(msg, 0)
		me := nMsg.ModelExit(nil)
		assert.NotNil(t, me, "trace[%d] should have model_exit", idx)
	}
}

func TestTimes(t *testing.T) {
	t.Skip()
}
