// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package convert

import (
	"fmt"
	"log/slog"

	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"
	"github.com/boschglobal/dse.schemas/code/go/ab/stream/pdu"
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/notify"
	flatbuffers "github.com/google/flatbuffers/go"
)

type Asc struct {
	traceFile string
	txEcuId   uint32

	// SimBus trace.
	channels map[string]*binaryChannel
	// NCodec trace.
	measurement *NetworkMeasurement
	// Active trace (one of channels, or measurement).
	active struct {
		measurement    *NetworkMeasurement
		simulationTime float64
	}
}

const fileIdentifierLength = 4
const pduStreamID = "SPDU"

func NewAsc(traceFile string, name string, txEcuId uint32) *Asc {
	return &Asc{
		traceFile: traceFile,
		txEcuId:   txEcuId,
		channels:  make(map[string]*binaryChannel),
		measurement: func() *NetworkMeasurement {
			if name != "" {
				return NewNetworkMeasurement(name, traceFile, txEcuId)
			} else {
				return nil
			}
		}(),
	}
}

func (a *Asc) Close() {
	for _, c := range a.channels {
		for _, n := range c.networks {
			if n.file != nil {
				n.writeBackMatter()
				if err := n.file.Close(); err != nil {
					panic(err)
				}
			}
		}
	}
	if a.measurement != nil {
		if a.measurement.file != nil {
			a.measurement.writeBackMatter()
			if err := a.measurement.file.Close(); err != nil {
				panic(err)
			}
		}
	}
}

func (a *Asc) VisitNotifyMsg(nm trace.NotifyMsg) {
	// Embedded ModelRegister message.
	if mReg := nm.Msg.ModelRegister(nil); mReg != nil {
		return // Discard
	}
	// Embedded ModelExit message.
	if mExit := nm.Msg.ModelExit(nil); mExit != nil {
		return // Discard
	}
	// Signal Index from SimBus.
	if siMsg := nm.Msg.SignalIndex(nil); siMsg != nil {
		a.handleSignalIndex(nm)
		return
	}

	// Notify with Signal Vectors (if not the above messages, then its this).
	if nm.Msg.ModelUidLength() > 0 {
		return // Discard, message from model.
	}

	for i := range nm.Msg.SignalsLength() {
		sv := new(notify.SignalVector)
		if ok := nm.Msg.Signals(sv, i); !ok {
			slog.Error(fmt.Sprintf("  <unable to decode SignalVector (i=%d)>", i))
			continue
		}

		// Locate the channel.
		chName := string(sv.Name())
		ch, ok := a.channels[chName]
		if !ok {
			slog.Error(fmt.Sprintf("  <unable to locate channel (%s)>", chName))
			continue
		}

		//
		for j := range sv.BinarySignalLength() {
			s := new(notify.BinarySignal)
			if ok := sv.BinarySignal(s, j); !ok {
				slog.Error(fmt.Sprintf("  <unable to decode signal (j=%d)>\n", j))
				continue
			}
			a.handleBinarySignal(nm, ch, s)
		}
	}
}

func (a *Asc) VisitStream(data []byte) {
	if a.active.measurement == nil {
		if a.measurement != nil {
			a.active.measurement = a.measurement
		} else {
			slog.Debug("No active measurement")
			return
		}
	}
	if len(data) == 0 {
		return
	}
	offset := flatbuffers.UOffsetT(0)
	for offset < flatbuffers.UOffsetT(len(data)) {
		if len(data[offset:]) < flatbuffers.SizeUint32+fileIdentifierLength {
			slog.Debug("buffer too short for FlatBuffer identifier")
			return
		}
		size := flatbuffers.GetUint32(data[offset:])
		if size > (uint32(len(data)) - uint32(offset) - flatbuffers.SizeUint32) {
			slog.Debug("FBS size exceeds stream length", "size", size, "offset", offset, "len", len(data))
			return
		}
		id := flatbuffers.GetSizePrefixedBufferIdentifier(data[offset:])
		switch id {
		case pduStreamID:
			stream := pdu.GetSizePrefixedRootAsStream(data, offset)
			sst := stream.SimulationTime()
			if sst == 0.0 {
				sst = a.active.simulationTime
			}
			for i := 0; i < stream.PdusLength(); i++ {
				pdu := new(pdu.Pdu)
				stream.Pdus(pdu, i)
				a.active.measurement.writeMessageEvent(sst, pdu)
			}

		default:
			slog.Error("FBS Identifier unknown", "id", id)
		}
		offset += flatbuffers.SizeUint32 + flatbuffers.UOffsetT(size)
	}
}

func (a *Asc) handleSignalIndex(nm trace.NotifyMsg) {
	siMsg := nm.Msg.SignalIndex(nil)
	if siMsg == nil {
		return
	}

	chName := string(nm.Msg.ChannelName())
	ch, ok := a.channels[chName]
	if !ok {
		ch = newBinaryChannel(chName, a.traceFile)
		a.channels[ch.name] = ch
	}
	// Extract Signal UIDs and add to index.
	for i := range nm.Msg.SignalIndex(nil).IndexesLength() {
		sl := new(notify.SignalLookup)
		siMsg.Indexes(sl, i)
		uid := sl.SignalUid()
		if uid != 0 {
			if _, ok := ch.networks[uid]; !ok {
				n := NewNetworkMeasurement(string(sl.Name()), a.traceFile, a.txEcuId)
				ch.networks[uid] = n
				slog.Debug(fmt.Sprintf(" network indexed: %s, %d", string(sl.Name()), uid))
			}
		}
	}
}

func (a *Asc) handleBinarySignal(nm trace.NotifyMsg, ch *binaryChannel, s *notify.BinarySignal) {
	if s.DataLength() == 0 {
		return
	}
	n, ok := ch.networks[s.Uid()]
	if !ok {
		slog.Error("Network not found", "uid", s.Uid())
		return
	}
	a.active.measurement = n
	a.active.simulationTime = nm.Msg.ModelTime()
	a.VisitStream(s.DataBytes())
	a.active.measurement = nil
}

type binaryChannel struct {
	name     string
	networks map[uint32]*NetworkMeasurement // Signal UID -> network signal/trace
}

func newBinaryChannel(name string, tracefile string) *binaryChannel {
	c := &binaryChannel{
		name:     name,
		networks: make(map[uint32]*NetworkMeasurement),
	}

	return c
}
