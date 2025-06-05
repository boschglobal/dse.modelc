// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package model

import (
	"bytes"
	"fmt"
	"log/slog"
	"maps"
	"slices"

	flatbuffers "github.com/google/flatbuffers/go"
	"github.com/vmihailenco/msgpack/v5"

	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/errors"
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/channel"
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/notify"
)

const channelMessageIdentifier string = "SBCH"
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

func (msg *Message) sendChannelMsg(gw *Gateway, channelName string,
	chMsg flatbuffers.UOffsetT, chMsgType channel.MessageType, withToken bool) (token int32) {

	token = 0
	channel.ChannelMessageStart(msg.builder)
	channel.ChannelMessageAddModelUid(msg.builder, gw.Uid)
	channel.ChannelMessageAddMessageType(msg.builder, chMsgType)
	channel.ChannelMessageAddMessage(msg.builder, chMsg)
	if withToken {
		token = gw.connection.Token()
		channel.ChannelMessageAddToken(msg.builder, token)
	}
	cm := channel.ChannelMessageEnd(msg.builder)

	msg.builder.FinishSizePrefixedWithFileIdentifier(cm, []byte(channelMessageIdentifier))
	msg.buffer = msg.builder.FinishedBytes()
	gw.connection.SendMessage(msg.buffer, channelName)

	return
}

func (msg *Message) sendNotifyMsg(gw *Gateway, nMsg flatbuffers.UOffsetT) {
	msg.builder.FinishSizePrefixedWithFileIdentifier(nMsg, []byte(notifyMessageIdentifier))
	msg.buffer = msg.builder.FinishedBytes()

	gw.connection.SendMessage(msg.buffer, "")
}

func (msg *Message) ModelRegister(gw *Gateway) (tokens []int32) {
	tokens = []int32{}
	buildMsg := func() flatbuffers.UOffsetT {
		msg.reset()
		channel.ModelRegisterStart(msg.builder)
		channel.ModelRegisterAddModelUid(msg.builder, gw.Uid)
		channel.ModelRegisterAddNotifyUid(msg.builder, gw.Uid)
		return channel.ModelReadyEnd(msg.builder)
	}

	for name, _ := range gw.ScalarSignalVector {
		slog.Debug(fmt.Sprintf("ModelRegister --> [%s]", name))
		token := msg.sendChannelMsg(gw, name, buildMsg(), channel.MessageTypeModelRegister, true)
		tokens = append(tokens, token)
	}
	for name, _ := range gw.BinarySignalVector {
		slog.Debug(fmt.Sprintf("ModelRegister --> [%s]", name))
		token := msg.sendChannelMsg(gw, name, buildMsg(), channel.MessageTypeModelRegister, true)
		tokens = append(tokens, token)
	}
	return
}

func (msg *Message) ModelExit(gw *Gateway) {
	buildMsg := func() flatbuffers.UOffsetT {
		msg.reset()
		channel.ModelExitStart(msg.builder)
		return channel.ModelExitEnd(msg.builder)
	}

	for name, _ := range gw.ScalarSignalVector {
		slog.Debug(fmt.Sprintf("ModelExit --> [%s]", name))
		msg.sendChannelMsg(gw, name, buildMsg(), channel.MessageTypeModelExit, false)
	}
	for name, _ := range gw.BinarySignalVector {
		slog.Debug(fmt.Sprintf("ModelExit --> [%s]", name))
		msg.sendChannelMsg(gw, name, buildMsg(), channel.MessageTypeModelExit, false)
	}
}

func (msg *Message) SignalIndex(gw *Gateway) {
	buildMsg := func(names []string) flatbuffers.UOffsetT {
		msg.reset()
		slVec := []flatbuffers.UOffsetT{}
		for _, name := range names {
			signalName := msg.builder.CreateString(name)
			channel.SignalLookupStart(msg.builder)
			channel.SignalLookupAddName(msg.builder, signalName)
			slVec = append(slVec, channel.SignalLookupEnd(msg.builder))
		}
		slVecOffset := msg.builder.CreateVectorOfTables(slVec)
		channel.SignalIndexStart(msg.builder)
		channel.SignalIndexAddIndexes(msg.builder, slVecOffset)
		return channel.SignalIndexEnd(msg.builder)
	}

	for _, sv := range gw.ScalarSignalVector {
		slog.Debug(fmt.Sprintf("SignalIndex --> [%s]", sv.Name))
		msg.sendChannelMsg(gw, sv.Name,
			buildMsg(slices.Collect(maps.Keys(sv.Index.Name))),
			channel.MessageTypeSignalIndex, false)
	}

	for _, sv := range gw.BinarySignalVector {
		slog.Debug(fmt.Sprintf("SignalIndex --> [%s]", sv.Name))
		msg.sendChannelMsg(gw, sv.Name,
			buildMsg(slices.Collect(maps.Keys(sv.Index.Name))),
			channel.MessageTypeSignalIndex, false)
	}
}

func (msg *Message) SignalRead(gw *Gateway) {
	buildMsg := func(names []string) flatbuffers.UOffsetT {
		buf := new(bytes.Buffer)
		enc := msgpack.NewEncoder(buf)
		enc.EncodeArrayLen(1)
		enc.EncodeArrayLen(len(names))
		for _, name := range names {
			enc.EncodeString(name)
		}
		msg.reset()
		mpOffset := msg.builder.CreateByteVector(buf.Bytes())
		channel.SignalReadStart(msg.builder)
		channel.SignalReadAddData(msg.builder, mpOffset)
		return channel.SignalReadEnd(msg.builder)
	}

	for _, sv := range gw.ScalarSignalVector {
		slog.Debug(fmt.Sprintf("SignalRead --> [%s]", sv.Name))
		msg.sendChannelMsg(gw, sv.Name,
			buildMsg(slices.Collect(maps.Keys(sv.Index.Name))),
			channel.MessageTypeSignalRead, false)
	}
	for _, sv := range gw.BinarySignalVector {
		slog.Debug(fmt.Sprintf("SignalRead --> [%s]", sv.Name))
		msg.sendChannelMsg(gw, sv.Name,
			buildMsg(slices.Collect(maps.Keys(sv.Index.Name))),
			channel.MessageTypeSignalRead, false)
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
		nameOffset := msg.builder.CreateString(sv.Name)
		mpBuf, _ := sv.toMsgPack()
		mpOffset := msg.builder.CreateByteVector(mpBuf.Bytes())
		notify.SignalVectorStart(msg.builder)
		notify.SignalVectorAddName(msg.builder, nameOffset)
		notify.SignalVectorAddModelUid(msg.builder, gw.Uid)
		notify.SignalVectorAddData(msg.builder, mpOffset)
		svOffset := notify.SignalVectorEnd(msg.builder)
		svVev = append(svVev, svOffset)
	}

	signalVectorVector := msg.builder.CreateVectorOfTables(svVev)

	// Model UID vector.
	// TODO refactor (somehow) the UID Vector to be variable length.
	// For GW its always 1, but for reuse of code, support a vector.
	notify.NotifyMessageStartModelUidVector(msg.builder, 1)
	msg.builder.PlaceUint32(gw.Uid)
	modelUidVector := msg.builder.EndVector(1)

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

	if msg.buildOnly == false {
		msg.sendNotifyMsg(gw, nMsg)
	}
	return nMsg
}

func HandleChannelMessage(gw *Gateway, channelName string, msg []byte) (err error) {
	if !flatbuffers.BufferHasIdentifier(msg[4:], channelMessageIdentifier) {
		return errors.ErrModelUnexpectedMessageId(channelMessageIdentifier)
	}
	cm := channel.GetSizePrefixedRootAsChannelMessage(msg, 0)
	if cm.ModelUid() != gw.Uid {
		slog.Debug(fmt.Sprintf("Message not for this model, continuing to wait (message uid=%d)", cm.ModelUid()))
		return nil
	}

	switch cm.MessageType() {
	case channel.MessageTypeModelRegister:
	case channel.MessageTypeModelReady:
	case channel.MessageTypeModelStart:
	case channel.MessageTypeModelExit:
	case channel.MessageTypeSignalIndex:
		slog.Debug(fmt.Sprintf("SignalIndex <-- [%s]", channelName))
		ut := new(flatbuffers.Table)
		if ok := cm.Message(ut); !ok {
			return errors.ErrModelMessageDecode("unable to decode SignalIndex")
		}
		names := []string{}
		uids := []uint32{}
		siMsg := new(channel.SignalIndex)
		siMsg.Init(ut.Bytes, ut.Pos)
		for i := range siMsg.IndexesLength() {
			slTable := new(channel.SignalLookup)
			if ok := siMsg.Indexes(slTable, i); !ok {
				return errors.ErrModelMessageDecode(fmt.Sprintf("unable to decode SignalLookup (i=%d)", i))
			}
			names = append(names, string(slTable.Name()))
			uids = append(uids, slTable.SignalUid())
		}
		// FIXME need to ensure that channel name is unique over both scalar and binary.
		if sv, ok := gw.ScalarSignalVector[channelName]; ok {
			sv.indexSignals(names, uids)
		} else if sv, ok := gw.BinarySignalVector[channelName]; ok {
			sv.indexSignals(names, uids)
		} else {
			return errors.ErrModelMissingSignalVector(channelName)
		}
	case channel.MessageTypeSignalRead:
	case channel.MessageTypeSignalValue:
		slog.Debug(fmt.Sprintf("SignalValue <-- [%s]", channelName))
		ut := new(flatbuffers.Table)
		if ok := cm.Message(ut); !ok {
			return errors.ErrModelMessageDecode("unable to decode SignalValue")
		}
		svMsg := new(channel.SignalValue)
		svMsg.Init(ut.Bytes, ut.Pos)

		if sv, ok := gw.ScalarSignalVector[channelName]; ok {
			buf := bytes.NewBuffer(svMsg.DataBytes())
			if err := sv.fromMsgPack(buf); err != nil {
				slog.Debug(fmt.Sprint("SignalValue not decoded, err: ", err))
			}
		} else if _, ok := gw.BinarySignalVector[channelName]; ok {
			// Currently binary data is not expected in a SV message.
		} else {
			return errors.ErrModelMissingSignalVector(channelName)
		}
	case channel.MessageTypeSignalWrite:
	case channel.MessageTypeNONE:
	default:
	}

	return nil
}

func HandleNotifyMessage(gw *Gateway, msg []byte) (discard bool, err error) {
	if !flatbuffers.BufferHasIdentifier(msg[4:], notifyMessageIdentifier) {
		return false, errors.ErrModelUnexpectedMessageId(notifyMessageIdentifier)
	}
	nMsg := notify.GetSizePrefixedRootAsNotifyMessage(msg, 0)

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
			buf := bytes.NewBuffer(nSv.DataBytes())
			if err := sv.fromMsgPack(buf); err != nil {
				slog.Debug(fmt.Sprint("SignalVector not decoded, err: ", err))
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
		msg, channelName, err := gw.connection.WaitMessage(false)
		if err != nil {
			return false, err
		}
		// FIXME loop over more than one message in buffer?
		if len(channelName) > 0 {
			if err := HandleChannelMessage(gw, channelName, msg); err != nil {
				slog.Debug(fmt.Sprint("Error while waiting for Notify:", err))
			}
		} else {
			discard, err := HandleNotifyMessage(gw, msg)
			if discard {
				continue
			}
			return true, err
		}
	}
}

func WaitForChannelMessage(gw *Gateway, msgType channel.MessageType) (name string, err error, token int32) {
	for {
		immediate := false
		if msgType == channel.MessageTypeModelRegister {
			immediate = true
		}
		msg, channelName, err := gw.connection.WaitMessage(immediate)
		if err != nil {
			return "", err, 0
		}
		// FIXME loop over more than one message in buffer?
		if len(channelName) > 0 {
			if !flatbuffers.BufferHasIdentifier(msg[4:], channelMessageIdentifier) {
				slog.Debug(fmt.Sprint("Error, unexpected message identifier, continuing to wait!"))
				continue
			}
			cm := channel.GetSizePrefixedRootAsChannelMessage(msg, 0)
			if cm.ModelUid() != gw.Uid {
				slog.Debug(fmt.Sprintf("Message not for this model, continuing to wait (message uid=%d, model uid=%d)", cm.ModelUid(), gw.Uid))
				continue
			}
			if token := cm.Token(); token > 0 {
				//if (token > 0) && (token == cm.Token()) {
				// This is an Ack message.
				return channelName, nil, token
			}
			if err := HandleChannelMessage(gw, channelName, msg); err != nil {
				return "", err, 0
			}
			if cm.MessageType() == msgType {
				return channelName, nil, 0
			}
		} else {
			discard, err := HandleNotifyMessage(gw, msg)
			if discard {
				continue
			}
			if err != nil {
				slog.Debug(fmt.Sprint("Error while waiting for Channel Msg:", err))
			}
		}
	}
}
