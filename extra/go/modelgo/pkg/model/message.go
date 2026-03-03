// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package model

import (
	"fmt"
	"log/slog"
	"maps"
	"slices"

	flatbuffers "github.com/google/flatbuffers/go"

	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/errors"
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/notify"
)

const notifyMessageIdentifier string = "SBNO"

type Message struct {
	buffer  []byte
	builder *flatbuffers.Builder

	buildOnly bool
}

func (msg *Message) reset() {
	if msg.builder == nil {
		msg.builder = flatbuffers.NewBuilder(1024)
	}
	msg.builder.Reset()
}

func (msg *Message) sendNotifyMsg(gw *Gateway, nMsg flatbuffers.UOffsetT) {
	msg.builder.FinishSizePrefixedWithFileIdentifier(nMsg, []byte(notifyMessageIdentifier))
	msg.buffer = msg.builder.FinishedBytes()
	gw.connection.SendMessage(msg.buffer)
}

func (msg *Message) buildModelUidVector(uid uint32) flatbuffers.UOffsetT {
	notify.NotifyMessageStartModelUidVector(msg.builder, 1)
	msg.builder.PrependUint32(uid)
	return msg.builder.EndVector(1)
}

// ModelRegister sends a NotifyMessage(SBNO) containing a ModelRegister table
// for each channel, requesting an ACK token per channel.
func (msg *Message) ModelRegister(gw *Gateway) (tokens []int32) {
	tokens = []int32{}

	sendForChannel := func(chName string) {
		msg.reset()

		token := gw.connection.Token()

		// ModelRegister sub-table.
		notify.ModelRegisterStart(msg.builder)
		notify.ModelRegisterAddModelUid(msg.builder, gw.Uid)
		notify.ModelRegisterAddNotifyUid(msg.builder, gw.Uid)
		mrOffset := notify.ModelRegisterEnd(msg.builder)

		// model_uid vector.
		modelUidVec := msg.buildModelUidVector(gw.Uid)

		// channel_name string.
		chNameOffset := msg.builder.CreateString(chName)

		// NotifyMessage container.
		notify.NotifyMessageStart(msg.builder)
		notify.NotifyMessageAddToken(msg.builder, token)
		notify.NotifyMessageAddModelUid(msg.builder, modelUidVec)
		notify.NotifyMessageAddChannelName(msg.builder, chNameOffset)
		notify.NotifyMessageAddModelRegister(msg.builder, mrOffset)
		nMsg := notify.NotifyMessageEnd(msg.builder)

		slog.Debug(fmt.Sprintf("ModelRegister --> [%s]", chName))
		msg.sendNotifyMsg(gw, nMsg)
		tokens = append(tokens, token)
	}

	for name := range gw.ScalarSignalVector {
		sendForChannel(name)
	}
	for name := range gw.BinarySignalVector {
		sendForChannel(name)
	}
	return
}

// ModelExit sends a NotifyMessage(SBNO) containing a ModelExit table for each
// channel.
func (msg *Message) ModelExit(gw *Gateway) {
	sendForChannel := func(chName string) {
		msg.reset()

		// model_uid vector.
		modelUidVec := msg.buildModelUidVector(gw.Uid)

		// channel_name string.
		chNameOffset := msg.builder.CreateString(chName)

		// ModelExit sub-table.
		notify.ModelExitStart(msg.builder)
		meOffset := notify.ModelExitEnd(msg.builder)

		// NotifyMessage container.
		notify.NotifyMessageStart(msg.builder)
		notify.NotifyMessageAddModelUid(msg.builder, modelUidVec)
		notify.NotifyMessageAddChannelName(msg.builder, chNameOffset)
		notify.NotifyMessageAddModelExit(msg.builder, meOffset)
		nMsg := notify.NotifyMessageEnd(msg.builder)

		slog.Debug(fmt.Sprintf("ModelExit --> [%s]", chName))
		msg.sendNotifyMsg(gw, nMsg)
	}

	for name := range gw.ScalarSignalVector {
		sendForChannel(name)
	}
	for name := range gw.BinarySignalVector {
		sendForChannel(name)
	}
}

// SignalIndex sends a NotifyMessage(SBNO) containing a SignalIndex table for
// each channel.
func (msg *Message) SignalIndex(gw *Gateway) {
	sendSignalIndex := func(chName string, signalNames []string) {
		msg.reset()

		token := gw.connection.Token()

		// SignalLookup vector.
		slVec := []flatbuffers.UOffsetT{}
		for _, name := range signalNames {
			nameOff := msg.builder.CreateString(name)
			notify.SignalLookupStart(msg.builder)
			notify.SignalLookupAddName(msg.builder, nameOff)
			slVec = append(slVec, notify.SignalLookupEnd(msg.builder))
		}
		slVecOffset := msg.builder.CreateVectorOfTables(slVec)

		// SignalIndex sub-table.
		notify.SignalIndexStart(msg.builder)
		notify.SignalIndexAddIndexes(msg.builder, slVecOffset)
		siOffset := notify.SignalIndexEnd(msg.builder)

		// model_uid vector.
		modelUidVec := msg.buildModelUidVector(gw.Uid)

		// channel_name string.
		chNameOffset := msg.builder.CreateString(chName)

		// NotifyMessage container.
		notify.NotifyMessageStart(msg.builder)
		notify.NotifyMessageAddToken(msg.builder, token)
		notify.NotifyMessageAddModelUid(msg.builder, modelUidVec)
		notify.NotifyMessageAddChannelName(msg.builder, chNameOffset)
		notify.NotifyMessageAddSignalIndex(msg.builder, siOffset)
		nMsg := notify.NotifyMessageEnd(msg.builder)

		slog.Debug(fmt.Sprintf("SignalIndex --> [%s]", chName))
		msg.sendNotifyMsg(gw, nMsg)
	}

	for _, sv := range gw.ScalarSignalVector {
		sendSignalIndex(sv.Name, slices.Collect(maps.Keys(sv.Index.Name)))
	}
	for _, sv := range gw.BinarySignalVector {
		sendSignalIndex(sv.Name, slices.Collect(maps.Keys(sv.Index.Name)))
	}
}

func (msg *Message) Notify(gw *Gateway) flatbuffers.UOffsetT {
	msg.reset()

	// SV vector.
	svVev := []flatbuffers.UOffsetT{}
	for _, sv := range gw.ScalarSignalVector {
		slog.Debug(fmt.Sprintf("Notify --> [%s]", sv.Name))
		delta := slices.Collect(maps.Keys(sv.delta.changed))
		if len(delta) == 0 {
			continue
		}
		notify.SignalVectorStartSignalVector(msg.builder, len(delta))
		for _, i := range delta {
			notify.CreateSignal(msg.builder, sv.Signal[i].Uid, sv.Signal[i].Value)
		}
		sVecOffset := msg.builder.EndVector(len(delta))
		nameOffset := msg.builder.CreateString(sv.Name)
		notify.SignalVectorStart(msg.builder)
		notify.SignalVectorAddName(msg.builder, nameOffset)
		notify.SignalVectorAddModelUid(msg.builder, gw.Uid)
		notify.SignalVectorAddSignal(msg.builder, sVecOffset)
		svOffset := notify.SignalVectorEnd(msg.builder)
		svVev = append(svVev, svOffset)
	}

	for _, sv := range gw.BinarySignalVector {
		slog.Debug(fmt.Sprintf("Notify --> [%s]", sv.Name))
		// Build BinarySignal entries for each signal in the vector.
		bsVec := []flatbuffers.UOffsetT{}
		for _, sig := range sv.Signal {
			dataOffset := msg.builder.CreateByteVector(sig.Value)
			notify.BinarySignalStart(msg.builder)
			notify.BinarySignalAddUid(msg.builder, sig.Uid)
			notify.BinarySignalAddData(msg.builder, dataOffset)
			bsVec = append(bsVec, notify.BinarySignalEnd(msg.builder))
		}
		bsVecOffset := msg.builder.CreateVectorOfTables(bsVec)
		nameOffset := msg.builder.CreateString(sv.Name)
		notify.SignalVectorStart(msg.builder)
		notify.SignalVectorAddName(msg.builder, nameOffset)
		notify.SignalVectorAddModelUid(msg.builder, gw.Uid)
		notify.SignalVectorAddBinarySignal(msg.builder, bsVecOffset)
		svOffset := notify.SignalVectorEnd(msg.builder)
		svVev = append(svVev, svOffset)
	}

	signalVectorVector := msg.builder.CreateVectorOfTables(svVev)

	// Model UID vector.
	modelUidVector := msg.buildModelUidVector(gw.Uid)

	// NotifyMessage message.
	notify.NotifyMessageStart(msg.builder)
	notify.NotifyMessageAddSignals(msg.builder, signalVectorVector)
	notify.NotifyMessageAddModelUid(msg.builder, modelUidVector)
	if gw.modelTime >= 0 {
		notify.NotifyMessageAddModelTime(msg.builder, gw.modelTime)
	}
	if gw.scheduleTime >= 0 {
		notify.NotifyMessageAddScheduleTime(msg.builder, gw.scheduleTime)
	}
	nMsg := notify.NotifyMessageEnd(msg.builder)

	if !msg.buildOnly {
		msg.sendNotifyMsg(gw, nMsg)
	}
	return nMsg
}

func handleNotifySignalIndex(gw *Gateway, channelName string, si *notify.SignalIndex) error {
	names := []string{}
	uids := []uint32{}
	for i := range si.IndexesLength() {
		slTable := new(notify.SignalLookup)
		if ok := si.Indexes(slTable, i); !ok {
			return errors.ErrModelMessageDecode(fmt.Sprintf("unable to decode SignalLookup (i=%d)", i))
		}
		names = append(names, string(slTable.Name()))
		uids = append(uids, slTable.SignalUid())
	}
	if sv, ok := gw.ScalarSignalVector[channelName]; ok {
		sv.indexSignals(names, uids)
	} else if sv, ok := gw.BinarySignalVector[channelName]; ok {
		sv.indexSignals(names, uids)
	} else {
		return errors.ErrModelMissingSignalVector(channelName)
	}
	return nil
}

// HandleNotifyMessage processes an inbound SBNO NotifyMessage.
func HandleNotifyMessage(gw *Gateway, msg []byte) (discard bool, err error) {
	if !flatbuffers.BufferHasIdentifier(msg[4:], notifyMessageIdentifier) {
		return false, errors.ErrModelUnexpectedMessageId(notifyMessageIdentifier)
	}
	nMsg := notify.GetSizePrefixedRootAsNotifyMessage(msg, 0)

	// ACK messages carry a non-zero token — discard them in the step loop.
	if nMsg.Token() != 0 {
		return true, nil
	}

	// ModelRegister ACKs embedded without token: also discard.
	if nMsg.ModelRegister(nil) != nil {
		return true, nil
	}

	slog.Debug(fmt.Sprintf("Notify/ModelStart <-- [%d]", gw.Uid))

	gw.modelTime = nMsg.ModelTime()
	gw.scheduleTime = nMsg.ScheduleTime()
	if gw.scheduleTime <= gw.modelTime {
		slog.Debug(fmt.Sprint("Warning: schedule time is not greater than model time."))
	}
	slog.Debug(fmt.Sprintf("    model_uid=%d", gw.Uid))
	slog.Debug(fmt.Sprintf("    model_time=%f", gw.modelTime))
	slog.Debug(fmt.Sprintf("    stop_time=%f", gw.scheduleTime))

	for i := range nMsg.SignalsLength() {
		nSv := new(notify.SignalVector)
		if ok := nMsg.Signals(nSv, i); !ok {
			slog.Debug(fmt.Sprint("SignalVector not encoded correctly at index: ", i))
			continue
		}

		svName := string(nSv.Name())
		if sv, ok := gw.ScalarSignalVector[svName]; ok {
			slog.Debug(fmt.Sprintf("SignalVector <-- [%s]", svName))
			sUids := []uint32{}
			sValues := []float64{}
			for i := range nSv.SignalLength() {
				sTable := new(notify.Signal)
				if ok := nSv.Signal(sTable, i); !ok {
					slog.Debug(fmt.Sprint("SignalVector Signal not decoded"))
					continue
				}
				sUids = append(sUids, sTable.Uid())
				sValues = append(sValues, sTable.Value())
			}
			sv.setByUid(sUids, sValues)
		} else if sv, ok := gw.BinarySignalVector[svName]; ok {
			slog.Debug(fmt.Sprintf("SignalVector <-- [%s]", svName))
			for j := range nSv.BinarySignalLength() {
				bsTable := new(notify.BinarySignal)
				if ok := nSv.BinarySignal(bsTable, j); !ok {
					continue
				}
				sv.setByUid([]uint32{bsTable.Uid()}, [][]byte{bsTable.DataBytes()})
			}
		} else {
			slog.Debug(fmt.Sprint("SignalVector not present in this model, SV name: ", svName))
			continue
		}
	}

	return false, nil
}

func WaitForNotifyMessage(gw *Gateway) (found bool, err error) {
	for {
		msg, err := gw.connection.WaitMessage(false)
		if err != nil {
			return false, err
		}
		discard, err := HandleNotifyMessage(gw, msg)
		if discard {
			continue
		}
		return true, err
	}
}

func WaitForNotifyAck(gw *Gateway, token int32) (channelName string, err error) {
	for {
		msg, err := gw.connection.WaitMessage(true)
		if err != nil {
			return "", err
		}
		if !flatbuffers.BufferHasIdentifier(msg[4:], notifyMessageIdentifier) {
			slog.Debug("WaitForNotifyAck: unexpected message identifier, continuing")
			continue
		}
		nMsg := notify.GetSizePrefixedRootAsNotifyMessage(msg, 0)
		if nMsg.Token() == token {
			slog.Debug(fmt.Sprintf("ModelRegister ACK <-- token=%d channel=%s", token, string(nMsg.ChannelName())))
			return string(nMsg.ChannelName()), nil
		}
	}
}

func WaitForSignalIndexAck(gw *Gateway) (channelName string, err error) {
	for {
		msg, err := gw.connection.WaitMessage(true)
		if err != nil {
			return "", err
		}
		if !flatbuffers.BufferHasIdentifier(msg[4:], notifyMessageIdentifier) {
			slog.Debug("WaitForSignalIndexAck: unexpected message identifier, continuing")
			continue
		}
		nMsg := notify.GetSizePrefixedRootAsNotifyMessage(msg, 0)
		chName := string(nMsg.ChannelName())

		siTable := nMsg.SignalIndex(nil)
		if siTable != nil {
			slog.Debug(fmt.Sprintf("SignalIndex ACK <-- [%s]", chName))
			if err := handleNotifySignalIndex(gw, chName, siTable); err != nil {
				return "", err
			}
			return chName, nil
		}
		// ACK for ModelRegister or unrelated — discard.
	}
}
