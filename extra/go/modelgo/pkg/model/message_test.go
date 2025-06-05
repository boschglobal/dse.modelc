// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package model

import (
	"bytes"
	"fmt"
	"testing"

	flatbuffers "github.com/google/flatbuffers/go"
	"github.com/stretchr/testify/assert"
	"github.com/vmihailenco/msgpack/v5"

	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/connection"
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/channel"
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
		rxBuffer, chName, err := gw.connection.WaitMessage(false)
		printMessageDebug(t, rxBuffer)

		// Message container properties.
		assert.Nil(t, err)
		assert.Greater(t, len(rxBuffer), 0)
		assert.Equal(t, sv_name, chName)

		// Message content.
		cm := channel.GetSizePrefixedRootAsChannelMessage(rxBuffer, 0)
		assert.Equal(t, cm.MessageType(), channel.MessageTypeModelRegister)
		assert.Equal(t, cm.ModelUid(), gw.Uid)
		ut := new(flatbuffers.Table)
		assert.Equal(t, true, cm.Message(ut))
		mr := new(channel.ModelRegister)
		mr.Init(ut.Bytes, ut.Pos)
		assert.Equal(t, gw.Uid, mr.ModelUid())
		assert.Equal(t, gw.Uid, mr.NotifyUid())
	}
}

func TestExitMessage(t *testing.T) {
	gw := createGateway(t)
	msg := &Message{}
	msg.ModelExit(gw)

	for _, sv_name := range []string{"scalar", "binary"} {
		rxBuffer, chName, err := gw.connection.WaitMessage(false)
		printMessageDebug(t, rxBuffer)

		// Message container properties.
		assert.Nil(t, err)
		assert.Greater(t, len(rxBuffer), 0)
		assert.Equal(t, sv_name, chName)

		// Message content.
		cm := channel.GetSizePrefixedRootAsChannelMessage(rxBuffer, 0)
		assert.Equal(t, cm.MessageType(), channel.MessageTypeModelExit)
		assert.Equal(t, cm.ModelUid(), gw.Uid)
		ut := new(flatbuffers.Table)
		assert.Equal(t, true, cm.Message(ut))
		me := new(channel.ModelExit)
		me.Init(ut.Bytes, ut.Pos)
	}
}

func TestSignalIndex(t *testing.T) {
	gw := createGateway(t)
	msg := &Message{}
	msg.SignalIndex(gw)

	for _, sv_name := range []string{"scalar", "binary"} {
		rxBuffer, chName, err := gw.connection.WaitMessage(false)
		printMessageDebug(t, rxBuffer)

		// Message container properties.
		assert.Nil(t, err)
		assert.Greater(t, len(rxBuffer), 0)
		assert.Equal(t, sv_name, chName)

		// Message content.
		cm := channel.GetSizePrefixedRootAsChannelMessage(rxBuffer, 0)
		assert.Equal(t, cm.MessageType(), channel.MessageTypeSignalIndex)
		assert.Equal(t, cm.ModelUid(), gw.Uid)
		ut := new(flatbuffers.Table)
		assert.Equal(t, true, cm.Message(ut))
		si := new(channel.SignalIndex)
		si.Init(ut.Bytes, ut.Pos)

		switch sv_name {
		case "scalar":
			assert.Equal(t, 3, si.IndexesLength())
			for i := range si.IndexesLength() {
				slTable := new(channel.SignalLookup)
				if ok := si.Indexes(slTable, i); !ok {
					t.Fail()
				}
				assert.Contains(t, gw.ScalarSignalVector["scalar"].Index.Name, string(slTable.Name()))
			}
		case "binary":
			assert.Equal(t, 1, si.IndexesLength())
			for i := range si.IndexesLength() {
				slTable := new(channel.SignalLookup)
				if ok := si.Indexes(slTable, i); !ok {
					t.Fail()
				}
				assert.Contains(t, gw.BinarySignalVector["binary"].Index.Name, string(slTable.Name()))
			}
		}
	}
}

func TestSignalRead(t *testing.T) {
	gw := createGateway(t)
	msg := &Message{}
	msg.SignalRead(gw)

	for _, sv_name := range []string{"scalar", "binary"} {
		rxBuffer, chName, err := gw.connection.WaitMessage(false)
		printMessageDebug(t, rxBuffer)

		// Message container properties.
		assert.Nil(t, err)
		assert.Greater(t, len(rxBuffer), 0)
		assert.Equal(t, sv_name, chName)

		// Message content.
		cm := channel.GetSizePrefixedRootAsChannelMessage(rxBuffer, 0)
		assert.Equal(t, cm.MessageType(), channel.MessageTypeSignalRead)
		assert.Equal(t, cm.ModelUid(), gw.Uid)
		ut := new(flatbuffers.Table)
		assert.Equal(t, true, cm.Message(ut))
		sr := new(channel.SignalRead)
		sr.Init(ut.Bytes, ut.Pos)

		switch sv_name {
		case "scalar":
			assert.Greater(t, sr.DataLength(), 0)
			reader := bytes.NewReader(sr.DataBytes())
			dec := msgpack.NewDecoder(reader)
			cLen, _ := dec.DecodeArrayLen()
			assert.Equal(t, 1, cLen)
			signalCount, err := dec.DecodeArrayLen()
			assert.Nil(t, err)
			assert.Equal(t, 3, signalCount)
			assert.Equal(t, gw.ScalarSignalVector["scalar"].Count(), signalCount)
			for _ = range signalCount {
				name, err := dec.DecodeString()
				assert.Nil(t, err)
				assert.Contains(t, gw.ScalarSignalVector["scalar"].Index.Name, name)
			}
		case "binary":
			assert.Greater(t, sr.DataLength(), 0)
			reader := bytes.NewReader(sr.DataBytes())
			dec := msgpack.NewDecoder(reader)
			cLen, _ := dec.DecodeArrayLen()
			assert.Equal(t, 1, cLen)
			signalCount, err := dec.DecodeArrayLen()
			assert.Nil(t, err)
			assert.Equal(t, 1, signalCount)
			assert.Equal(t, gw.BinarySignalVector["binary"].Count(), signalCount)
			for _ = range signalCount {
				name, err := dec.DecodeString()
				assert.Nil(t, err)
				assert.Contains(t, gw.BinarySignalVector["binary"].Index.Name, name)
			}
		}
	}
}

func pushSignalIndex(gw *Gateway, stub *connection.StubConnection, channelName string, names []string, uids []uint32) {
	msg := &Message{}
	msg.reset()

	// Push a SignalIndex on the connection.
	slVec := []flatbuffers.UOffsetT{}
	for i := range len(names) {
		signalName := msg.builder.CreateString(names[i])
		channel.SignalLookupStart(msg.builder)
		channel.SignalLookupAddName(msg.builder, signalName)
		channel.SignalLookupAddSignalUid(msg.builder, uids[i])
		slVec = append(slVec, channel.SignalLookupEnd(msg.builder))
	}
	slVecOffset := msg.builder.CreateVectorOfTables(slVec)
	channel.SignalIndexStart(msg.builder)
	channel.SignalIndexAddIndexes(msg.builder, slVecOffset)
	siMsg := channel.SignalIndexEnd(msg.builder)

	channel.ChannelMessageStart(msg.builder)
	channel.ChannelMessageAddModelUid(msg.builder, gw.Uid)
	channel.ChannelMessageAddMessageType(msg.builder, channel.MessageTypeSignalIndex)
	channel.ChannelMessageAddMessage(msg.builder, siMsg)
	cm := channel.ChannelMessageEnd(msg.builder)

	msg.builder.FinishSizePrefixedWithFileIdentifier(cm, []byte(channelMessageIdentifier))
	msg.buffer = msg.builder.FinishedBytes()
	stub.PushMessage(msg.buffer, channelName)
}

func TestWaitSignalIndex(t *testing.T) {
	names := []string{"one", "two", "three"}
	uids := []uint32{1, 22, 333}

	gw := createGateway(t)
	gw.ScalarSignalVector["scalar"].SetByName(names, []float64{1.1, 2.2, 3.3})
	gw.ScalarSignalVector["scalar"].ClearChanged()
	gw.ScalarSignalVector["scalar"].ClearUpdated()
	msg := &Message{}
	msg.reset()

	// Push a SignalIndex message on the connection.
	pushSignalIndex(gw, gw.connection.(*connection.StubConnection), "scalar", names, uids)

	// Wait for the message
	name, err, _ := WaitForChannelMessage(gw, channel.MessageTypeSignalIndex)
	assert.Equal(t, "scalar", name)
	assert.Nil(t, err)

	// Check the signals were indexed.
	for i := range len(uids) {
		uidIdx := gw.ScalarSignalVector["scalar"].Index.Uid[uids[i]]
		nameIdx := gw.ScalarSignalVector["scalar"].Index.Name[names[i]]
		assert.Equal(t, uidIdx, nameIdx)
	}
}

func pushSignalValue(gw *Gateway, stub *connection.StubConnection, channelName string, mpBuf *bytes.Buffer) {
	msg := &Message{}
	msg.reset()

	mpOffset := msg.builder.CreateByteVector(mpBuf.Bytes())
	channel.SignalValueStart(msg.builder)
	channel.SignalValueAddData(msg.builder, mpOffset)
	svMsg := channel.SignalValueEnd(msg.builder)

	channel.ChannelMessageStart(msg.builder)
	channel.ChannelMessageAddModelUid(msg.builder, gw.Uid)
	channel.ChannelMessageAddMessageType(msg.builder, channel.MessageTypeSignalValue)
	channel.ChannelMessageAddMessage(msg.builder, svMsg)
	cm := channel.ChannelMessageEnd(msg.builder)

	msg.builder.FinishSizePrefixedWithFileIdentifier(cm, []byte(channelMessageIdentifier))
	msg.buffer = msg.builder.FinishedBytes()
	stub.PushMessage(msg.buffer, channelName)
}

func TestWaitSignalValue(t *testing.T) {
	names := []string{"one", "two", "three"}
	uids := []uint32{1, 22, 333}
	orig_values := []float64{1.1, 2.2, 3.3}
	new_values := []float64{111.1, 222.2, 333.3}

	gw := createGateway(t)
	gw.ScalarSignalVector["scalar"].SetByName(names, orig_values)
	gw.ScalarSignalVector["scalar"].indexSignals(names, uids)
	gw.ScalarSignalVector["scalar"].ClearChanged()
	gw.ScalarSignalVector["scalar"].ClearUpdated()
	msg := &Message{}
	msg.reset()

	// Currently ...
	// SignalValue includes only scalar values. binary will be 0 len.
	// Only sent during startup (before notify)
	// handler can ignore binary data for now.

	// Modify signal values and push a SignalValue message.
	gw.ScalarSignalVector["scalar"].SetByName(names, new_values)
	mpBuf, _ := gw.ScalarSignalVector["scalar"].toMsgPack()
	pushSignalValue(gw, gw.connection.(*connection.StubConnection), "scalar", mpBuf)

	// Restore the original values.
	gw.ScalarSignalVector["scalar"].SetByName(names, orig_values)
	gw.ScalarSignalVector["scalar"].ClearChanged()
	gw.ScalarSignalVector["scalar"].ClearUpdated()
	assert.Equal(t, 0, len(gw.ScalarSignalVector["scalar"].delta.changed))
	assert.Equal(t, 0, len(gw.ScalarSignalVector["scalar"].delta.updated))

	// Wait for the message, check the signal values were updated.
	name, err, _ := WaitForChannelMessage(gw, channel.MessageTypeSignalValue)
	assert.Equal(t, "scalar", name)
	assert.Nil(t, err)
	for i := range len(names) {
		nameIdx := gw.ScalarSignalVector["scalar"].Index.Name[names[i]]
		assert.Equal(t, new_values[i], gw.ScalarSignalVector["scalar"].Signal[nameIdx].Value)
		assert.Contains(t, gw.ScalarSignalVector["scalar"].delta.updated, nameIdx)
	}
	assert.Equal(t, 0, len(gw.ScalarSignalVector["scalar"].delta.changed))
	assert.Equal(t, 3, len(gw.ScalarSignalVector["scalar"].delta.updated))
}

func pushRegisterAck(gw *Gateway, stub *connection.StubConnection, channelName string, token int32) {
	msg := &Message{}
	msg.reset()

	// Push a register ack on the connection.
	channel.ModelRegisterStart(msg.builder)
	channel.ModelRegisterAddModelUid(msg.builder, gw.Uid)
	chMsg := channel.ModelReadyEnd(msg.builder)

	channel.ChannelMessageStart(msg.builder)
	channel.ChannelMessageAddModelUid(msg.builder, gw.Uid)
	channel.ChannelMessageAddMessageType(msg.builder, channel.MessageTypeModelRegister)
	channel.ChannelMessageAddMessage(msg.builder, chMsg)
	channel.ChannelMessageAddToken(msg.builder, token)
	cm := channel.ChannelMessageEnd(msg.builder)

	msg.builder.FinishSizePrefixedWithFileIdentifier(cm, []byte(channelMessageIdentifier))
	msg.buffer = msg.builder.FinishedBytes()
	stub.PushMessage(msg.buffer, channelName)
}

func TestWaitRegisterAck(t *testing.T) {
	gw := createGateway(t)

	pushRegisterAck(gw, gw.connection.(*connection.StubConnection), "scalar", 24)
	name, err, token := WaitForChannelMessage(gw, channel.MessageTypeNONE)
	assert.Equal(t, "scalar", name)
	assert.Equal(t, int32(24), token)
	assert.Nil(t, err)

	// Check the assigned UID was absorbed?
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

	rxBuffer, chName, err := gw.connection.WaitMessage(false)
	printMessageDebug(t, rxBuffer)
	assert.Nil(t, err)
	assert.Greater(t, len(rxBuffer), 0)
	assert.Equal(t, "", chName)

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
			assert.Nil(t, svTable.DataBytes())
			assert.Equal(t, 0, svTable.DataLength())
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
			for i := range len(bnames) {
				nameIdx := gw.BinarySignalVector[svName].Index.Name[bnames[i]]
				assert.Equal(t, []byte{}, gw.BinarySignalVector[svName].Signal[nameIdx].Value)
			}
			buf := bytes.NewBuffer(svTable.DataBytes())
			err = sv.fromMsgPack(buf)
			assert.Nil(t, err)
			for i := range len(bnames) {
				nameIdx := gw.BinarySignalVector[svName].Index.Name[bnames[i]]
				assert.Equal(t, bvalues[i], gw.BinarySignalVector[svName].Signal[nameIdx].Value)
			}
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
	stub.PushMessage(msg.buffer, "")
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
	gw.BinarySignalVector["binary"].SetByName(names, bvalues)
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
