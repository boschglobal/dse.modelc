// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package model

import (
	"fmt"
	"testing"

	flatbuffers "github.com/google/flatbuffers/go"
	"github.com/stretchr/testify/assert"

	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/connection"
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/notify"
)

func createGateway(t *testing.T) *Gateway {
	gw := NewGateway("foo", 42, &connection.StubConnection{SendToStack: true})
	gw.ScalarSignalVector["scalar"] = NewSignalVector[float64]("scalar").Add([]string{"one", "two", "three"})
	gw.BinarySignalVector["binary"] = NewSignalVector[[]byte]("binary").Add([]string{"four"})
	assert.Equal(t, uint32(42), gw.Uid)
	return gw
}

func printMessageDebug(t *testing.T, buffer []byte) {
	t.Logf("%d\n", len(buffer))
	t.Logf("%02x\n", len(buffer))
	t.Logf("%02x\n", buffer)
}

func TestRegisterMessage(t *testing.T) {
	gw := createGateway(t)
	msg := &Message{}
	msg.ModelRegister(gw)

	for _, sv_name := range []string{"scalar", "binary"} {
		rxBuffer, err := gw.connection.WaitMessage(false)
		printMessageDebug(t, rxBuffer)

		assert.Nil(t, err)
		assert.Greater(t, len(rxBuffer), 0)
		assert.True(t, flatbuffers.BufferHasIdentifier(rxBuffer[4:], notifyMessageIdentifier))

		nMsg := notify.GetSizePrefixedRootAsNotifyMessage(rxBuffer, 0)
		assert.NotEqual(t, int32(0), nMsg.Token())
		assert.Equal(t, sv_name, string(nMsg.ChannelName()))
		assert.Equal(t, 1, nMsg.ModelUidLength())
		assert.Equal(t, gw.Uid, nMsg.ModelUid(0))

		mr := nMsg.ModelRegister(nil)
		assert.NotNil(t, mr)
		assert.Equal(t, gw.Uid, mr.ModelUid())
		assert.Equal(t, gw.Uid, mr.NotifyUid())
	}
}

func TestExitMessage(t *testing.T) {
	gw := createGateway(t)
	msg := &Message{}
	msg.ModelExit(gw)

	for _, sv_name := range []string{"scalar", "binary"} {
		rxBuffer, err := gw.connection.WaitMessage(false)
		printMessageDebug(t, rxBuffer)

		assert.Nil(t, err)
		assert.Greater(t, len(rxBuffer), 0)
		assert.True(t, flatbuffers.BufferHasIdentifier(rxBuffer[4:], notifyMessageIdentifier))

		nMsg := notify.GetSizePrefixedRootAsNotifyMessage(rxBuffer, 0)
		assert.Equal(t, sv_name, string(nMsg.ChannelName()))
		me := nMsg.ModelExit(nil)
		assert.NotNil(t, me)
	}
}

func TestSignalIndex(t *testing.T) {
	gw := createGateway(t)
	msg := &Message{}
	msg.SignalIndex(gw)

	for _, sv_name := range []string{"scalar", "binary"} {
		rxBuffer, err := gw.connection.WaitMessage(false)
		printMessageDebug(t, rxBuffer)

		assert.Nil(t, err)
		assert.Greater(t, len(rxBuffer), 0)
		assert.True(t, flatbuffers.BufferHasIdentifier(rxBuffer[4:], notifyMessageIdentifier))

		nMsg := notify.GetSizePrefixedRootAsNotifyMessage(rxBuffer, 0)
		assert.Equal(t, sv_name, string(nMsg.ChannelName()))
		si := nMsg.SignalIndex(nil)
		assert.NotNil(t, si)

		switch sv_name {
		case "scalar":
			assert.Equal(t, 3, si.IndexesLength())
			for i := range si.IndexesLength() {
				slTable := new(notify.SignalLookup)
				if ok := si.Indexes(slTable, i); !ok {
					t.Fail()
				}
				assert.Contains(t, gw.ScalarSignalVector["scalar"].Index.Name, string(slTable.Name()))
			}
		case "binary":
			assert.Equal(t, 1, si.IndexesLength())
			for i := range si.IndexesLength() {
				slTable := new(notify.SignalLookup)
				if ok := si.Indexes(slTable, i); !ok {
					t.Fail()
				}
				assert.Contains(t, gw.BinarySignalVector["binary"].Index.Name, string(slTable.Name()))
			}
		}
	}
}

func pushRegisterAck(gw *Gateway, stub *connection.StubConnection, channelName string, token int32) {
	msg := &Message{}
	msg.reset()

	chNameOffset := msg.builder.CreateString(channelName)

	notify.NotifyMessageStart(msg.builder)
	notify.NotifyMessageAddToken(msg.builder, token)
	notify.NotifyMessageAddChannelName(msg.builder, chNameOffset)
	nMsg := notify.NotifyMessageEnd(msg.builder)

	msg.builder.FinishSizePrefixedWithFileIdentifier(nMsg, []byte(notifyMessageIdentifier))
	msg.buffer = msg.builder.FinishedBytes()
	stub.PushMessage(msg.buffer)
}

func pushSignalIndexAck(gw *Gateway, stub *connection.StubConnection, channelName string, names []string, uids []uint32) {
	msg := &Message{}
	msg.reset()

	slVec := []flatbuffers.UOffsetT{}
	for i := range len(names) {
		signalName := msg.builder.CreateString(names[i])
		notify.SignalLookupStart(msg.builder)
		notify.SignalLookupAddName(msg.builder, signalName)
		notify.SignalLookupAddSignalUid(msg.builder, uids[i])
		slVec = append(slVec, notify.SignalLookupEnd(msg.builder))
	}
	slVecOffset := msg.builder.CreateVectorOfTables(slVec)
	notify.SignalIndexStart(msg.builder)
	notify.SignalIndexAddIndexes(msg.builder, slVecOffset)
	siOffset := notify.SignalIndexEnd(msg.builder)

	chNameOffset := msg.builder.CreateString(channelName)

	notify.NotifyMessageStart(msg.builder)
	notify.NotifyMessageAddChannelName(msg.builder, chNameOffset)
	notify.NotifyMessageAddSignalIndex(msg.builder, siOffset)
	nMsg := notify.NotifyMessageEnd(msg.builder)

	msg.builder.FinishSizePrefixedWithFileIdentifier(nMsg, []byte(notifyMessageIdentifier))
	msg.buffer = msg.builder.FinishedBytes()
	stub.PushMessage(msg.buffer)
}

func TestWaitSignalIndex(t *testing.T) {
	names := []string{"one", "two", "three"}
	uids := []uint32{1, 22, 333}

	gw := createGateway(t)
	gw.ScalarSignalVector["scalar"].SetByName(names, []float64{1.1, 2.2, 3.3})
	gw.ScalarSignalVector["scalar"].ClearChanged()
	gw.ScalarSignalVector["scalar"].ClearUpdated()

	// Push a SignalIndex ACK message.
	pushSignalIndexAck(gw, gw.connection.(*connection.StubConnection), "scalar", names, uids)

	chName, err := WaitForSignalIndexAck(gw)
	assert.Equal(t, "scalar", chName)
	assert.Nil(t, err)

	for i := range len(uids) {
		uidIdx := gw.ScalarSignalVector["scalar"].Index.Uid[uids[i]]
		nameIdx := gw.ScalarSignalVector["scalar"].Index.Name[names[i]]
		assert.Equal(t, uidIdx, nameIdx)
	}
}

func TestWaitRegisterAck(t *testing.T) {
	gw := createGateway(t)

	pushRegisterAck(gw, gw.connection.(*connection.StubConnection), "scalar", 24)
	chName, err := WaitForNotifyAck(gw, 24)
	assert.Equal(t, "scalar", chName)
	assert.Nil(t, err)
	assert.Equal(t, uint32(42), gw.Uid)
}

func TestNotify(t *testing.T) {
	names := []string{"one", "two", "three"}
	uids := []uint32{1, 22, 333}
	values := []float64{1.1, 2.2, 3.3}
	zero := []float64{0.0, 0.0, 0.0}
	bnames := []string{"four"}
	buids := []uint32{4444}
	bvalues := [][]byte{[]byte("hello world")}

	gw := createGateway(t)
	gw.ScalarSignalVector["scalar"].SetByName(names, values)
	gw.ScalarSignalVector["scalar"].indexSignals(names, uids)
	gw.BinarySignalVector["binary"].SetByName(bnames, bvalues)
	gw.BinarySignalVector["binary"].indexSignals(bnames, buids)
	gw.modelTime = 0.005
	msg := &Message{}
	msg.Notify(gw)

	rxBuffer, err := gw.connection.WaitMessage(false)
	printMessageDebug(t, rxBuffer)
	assert.Nil(t, err)
	assert.Greater(t, len(rxBuffer), 0)

	nMsg := notify.GetRootAsNotifyMessage(rxBuffer, 0+4)
	assert.Equal(t, 0.005, nMsg.ModelTime())
	assert.Equal(t, 1, nMsg.ModelUidLength())
	assert.Equal(t, gw.Uid, nMsg.ModelUid(0))
	assert.Equal(t, 2, nMsg.SignalsLength())

	for i := range nMsg.SignalsLength() {
		svTable := new(notify.SignalVector)
		if ok := nMsg.Signals(svTable, i); !ok {
			t.Fail()
		}
		svName := string(svTable.Name())
		switch svName {
		case "scalar":
			sv := gw.ScalarSignalVector[svName]
			assert.NotNil(t, sv)
			sv.SetByName(names, zero)
			for i := range len(names) {
				nameIdx := gw.ScalarSignalVector[svName].Index.Name[names[i]]
				assert.Equal(t, 0.0, gw.ScalarSignalVector[svName].Signal[nameIdx].Value)
			}
			assert.Equal(t, len(values), svTable.SignalLength())
			sUids := []uint32{}
			sValues := []float64{}
			for i := range svTable.SignalLength() {
				sTable := new(notify.Signal)
				if ok := svTable.Signal(sTable, i); !ok {
					t.Fail()
				}
				sUids = append(sUids, sTable.Uid())
				sValues = append(sValues, sTable.Value())
			}
			gw.ScalarSignalVector[svName].setByUid(sUids, sValues)
			for i := range len(names) {
				nameIdx := gw.ScalarSignalVector[svName].Index.Name[names[i]]
				assert.Equal(t, values[i], gw.ScalarSignalVector[svName].Signal[nameIdx].Value)
			}

		case "binary":
			sv := gw.BinarySignalVector[svName]
			assert.NotNil(t, sv)
			err = sv.Reset()
			assert.Nil(t, err)
			// Binary signals encoded as BinarySignal sub-tables.
			assert.Equal(t, 1, svTable.BinarySignalLength())
			bsTable := new(notify.BinarySignal)
			if ok := svTable.BinarySignal(bsTable, 0); !ok {
				t.Fail()
			}
			assert.Equal(t, buids[0], bsTable.Uid())
			sv.setByUid([]uint32{bsTable.Uid()}, [][]byte{bsTable.DataBytes()})
			nameIdx := gw.BinarySignalVector[svName].Index.Name[bnames[0]]
			assert.Equal(t, bvalues[0], gw.BinarySignalVector[svName].Signal[nameIdx].Value)

		default:
			assert.Fail(t, "Unexpected signal vector name: %s", svName)
		}
	}
}

func pushNotify(gw *Gateway, stub *connection.StubConnection, modelTime float64, scheduleTime float64) {
	_gw := *gw // Shallow copy
	_gw.modelTime = modelTime
	_gw.scheduleTime = scheduleTime
	fmt.Printf("  modeltime = %f\n", _gw.modelTime)
	msg := &Message{buildOnly: true}
	nMsg := msg.Notify(&_gw)

	msg.builder.FinishSizePrefixedWithFileIdentifier(nMsg, []byte(notifyMessageIdentifier))
	msg.buffer = msg.builder.FinishedBytes()
	stub.PushMessage(msg.buffer)
}

func TestWaitNotify(t *testing.T) {
	names := []string{"one", "two", "three"}
	uids := []uint32{1, 22, 333}
	values := []float64{1.1, 2.2, 3.3}
	new_values := []float64{111.1, 222.2, 333.3}
	bnames := []string{"four"}
	buids := []uint32{4444}
	bvalues := [][]byte{[]byte("hello world")}
	new_bvalues := [][]byte{[]byte("foo bar")}

	gw := createGateway(t)
	gw.ScalarSignalVector["scalar"].indexSignals(names, uids)
	gw.BinarySignalVector["binary"].indexSignals(bnames, buids)
	gw.ScalarSignalVector["scalar"].ClearChanged()
	gw.BinarySignalVector["binary"].ClearChanged()
	gw.ScalarSignalVector["scalar"].SetByName(names, new_values)
	gw.BinarySignalVector["binary"].SetByName(bnames, new_bvalues)

	// Push a notify message on the connection.
	pushNotify(gw, gw.connection.(*connection.StubConnection), 0.0150, 0.0200)

	// Restore signal values.
	gw.ScalarSignalVector["scalar"].SetByName(names, values)
	gw.ScalarSignalVector["scalar"].ClearChanged()
	// Suppress unused variable warning for bvalues.
	_ = bvalues
	gw.BinarySignalVector["binary"].Reset()
	gw.BinarySignalVector["binary"].ClearChanged()

	// Wait for notify, check signal values were modified.
	found, err := WaitForNotifyMessage(gw)
	assert.Equal(t, true, found)
	assert.Nil(t, err)
	assert.Equal(t, 0.0150, gw.modelTime)
	assert.Equal(t, 0.0200, gw.scheduleTime)
	for i := range len(names) {
		nameIdx := gw.ScalarSignalVector["scalar"].Index.Name[names[i]]
		assert.Equal(t, new_values[i], gw.ScalarSignalVector["scalar"].Signal[nameIdx].Value)
	}
	for i := range len(bnames) {
		nameIdx := gw.BinarySignalVector["binary"].Index.Name[bnames[i]]
		assert.Equal(t, new_bvalues[i], gw.BinarySignalVector["binary"].Signal[nameIdx].Value)
	}
}
