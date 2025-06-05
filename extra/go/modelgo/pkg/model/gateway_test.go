// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package model

import (
	"maps"
	"slices"
	"testing"

	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/channel"
	flatbuffers "github.com/google/flatbuffers/go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/connection"
	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/errors"
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
			gw:  Gateway{Name: "foo", Uid: 42, ScalarSignalVector: map[string]*SignalVector[float64]{"foo": &SignalVector[float64]{}}},
			err: errors.ErrModelNoConnection,
		},
		{
			gw:  Gateway{Name: "foo", Uid: 42, BinarySignalVector: map[string]*SignalVector[[]byte]{"bar": &SignalVector[[]byte]{}}},
			err: errors.ErrModelNoConnection,
		},
	}
	for _, test := range tests {
		err := test.gw.Connect()
		assert.Equal(t, test.err, err)
	}
}

func checkTraceChannelMessage(t *testing.T, stub *connection.StubConnection, index uint, channelName string, chMsgType channel.MessageType) {
	assert.Equal(t, channelName, stub.Trace[index].Channel)
	msg := stub.Trace[index].Msg
	assert.True(t, flatbuffers.BufferHasIdentifier(msg[4:], channelMessageIdentifier))
	cm := channel.GetSizePrefixedRootAsChannelMessage(msg, 0)
	assert.Equal(t, chMsgType, cm.MessageType())
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

	pushRegisterAck(_gw, stub, "scalar", 1)
	pushRegisterAck(_gw, stub, "binary", 2)
	pushSignalIndex(_gw, stub, "scalar", names, uids)
	pushSignalIndex(_gw, stub, "binary", bnames, buids)
	mpBuf, _ := _gw.ScalarSignalVector["scalar"].toMsgPack()
	pushSignalValue(_gw, stub, "scalar", mpBuf)
	mpBuf, _ = _gw.BinarySignalVector["binary"].toMsgPack()
	pushSignalValue(_gw, stub, "binary", mpBuf)

	require.Len(t, stub.Stack, 6)
	require.Len(t, stub.Trace, 0)

	// Start the test with a new GW object, stub is now primed.
	gw := NewGateway("foo", 42, stub)
	gw.ScalarSignalVector["scalar"] = NewSignalVector[float64]("scalar").Add(names)
	gw.BinarySignalVector["binary"] = NewSignalVector[[]byte]("binary").Add(bnames)
	gw.RegisterRetry = 5
	err := gw.Connect()
	assert.Nil(t, err)

	// Check the UIDs.
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
	for i := range len(uids) {
		uidIdx := gw.ScalarSignalVector["scalar"].Index.Uid[uids[i]]
		assert.Equal(t, values[i], gw.ScalarSignalVector["scalar"].Signal[uidIdx].Value)
	}

	// Check the Trace.
	require.Len(t, stub.Stack, 0)
	require.Len(t, stub.Trace, 12)
	checkTraceChannelMessage(t, stub, 0, "scalar", channel.MessageTypeModelRegister)
	checkTraceChannelMessage(t, stub, 1, "binary", channel.MessageTypeModelRegister)
	checkTraceChannelMessage(t, stub, 2, "scalar", channel.MessageTypeModelRegister)
	checkTraceChannelMessage(t, stub, 3, "binary", channel.MessageTypeModelRegister)
	checkTraceChannelMessage(t, stub, 4, "scalar", channel.MessageTypeSignalIndex)
	checkTraceChannelMessage(t, stub, 5, "binary", channel.MessageTypeSignalIndex)
	checkTraceChannelMessage(t, stub, 6, "scalar", channel.MessageTypeSignalIndex)
	checkTraceChannelMessage(t, stub, 7, "binary", channel.MessageTypeSignalIndex)
	checkTraceChannelMessage(t, stub, 8, "scalar", channel.MessageTypeSignalRead)
	checkTraceChannelMessage(t, stub, 9, "binary", channel.MessageTypeSignalRead)
	checkTraceChannelMessage(t, stub, 10, "scalar", channel.MessageTypeSignalValue)
	checkTraceChannelMessage(t, stub, 11, "binary", channel.MessageTypeSignalValue)
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
	//checkTraceChannelMessage(t, stub, 0, "", channel.MessageTypeModelExit)
}

func TestExit(t *testing.T) {
	names := []string{"one", "two", "three"}
	//uids := []uint32{123, 456, 789}
	bnames := []string{"four"}
	//buids := []uint32{321}
	//values := []float64{1.1, 22.22, 333.333}

	stub := &connection.StubConnection{}
	gw := NewGateway("foo", 42, stub)
	gw.ScalarSignalVector["scalar"] = NewSignalVector[float64]("scalar").Add(names)
	gw.BinarySignalVector["binary"] = NewSignalVector[[]byte]("binary").Add(bnames)
	gw.Disconnect()

	// Check the Trace.
	require.Len(t, stub.Trace, 2)
	checkTraceChannelMessage(t, stub, 0, "scalar", channel.MessageTypeModelExit)
	checkTraceChannelMessage(t, stub, 1, "binary", channel.MessageTypeModelExit)

}

func TestTimes(t *testing.T) {
	t.Skip()
}
