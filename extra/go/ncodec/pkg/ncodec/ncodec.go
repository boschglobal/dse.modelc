// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package ncodec

import (
	"fmt"

	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/codec/automotivebus"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/message"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/trace"
)

type Codec[T message.MessageType] interface {
	Configure(MimeType string, stream *[]byte, trace trace.Trace[T]) error
	Read() (msg []*T, err error)
	Write(msg []*T) error
	Flush() error
	Truncate() error
	Stat(param string) (*string, error)
	Trace(t trace.Trace[T]) error
}

func NewTrace[T message.MessageType](codec Codec[T]) trace.Trace[T] {
	var msgType T

	switch any(msgType).(type) {
	case message.PduMessage:
		if swc_id, _ := codec.Stat("swc_id"); swc_id != nil {
			envName := fmt.Sprintf("NCODEC_TRACE_PDU_%s", *swc_id)
			td := new(trace.PduTraceData[T])
			td.Wildcard, td.Filter = trace.GetTraceEnv(envName)
			return td
		}
	case message.CanMessage:
		bus, _ := codec.Stat("bus")
		bus_id, _ := codec.Stat("bus_id")
		if bus != nil && bus_id != nil {
			envName := fmt.Sprintf("NCODEC_TRACE_CAN_%s_%s", *bus, *bus_id)
			td := new(trace.CanTraceData[T])
			td.Wildcard, td.Filter = trace.GetTraceEnv(envName)
			return td
		}
	}

	return nil
}

func NewNetworkCodec[T message.MessageType](MimeType string, stream *[]byte, ModelName string, SimulationTime *float64) (Codec[T], error) {
	var msgType T
	var codec Codec[T]

	switch any(msgType).(type) {
	case message.PduMessage:
		pduCodec := new(automotivebus.PduCodec[T])
		pduCodec.ModelName = ModelName
		pduCodec.SimulationTime = SimulationTime
		codec = pduCodec
	case message.CanMessage:
		canCodec := new(automotivebus.CanCodec[T])
		canCodec.ModelName = ModelName
		canCodec.SimulationTime = SimulationTime
		codec = canCodec
	default:
		return nil, fmt.Errorf("unsupported message type: %T", msgType)
	}

	if err := codec.Configure(MimeType, stream, NewTrace(codec)); err != nil {
		return nil, err
	}
	return codec, nil
}
