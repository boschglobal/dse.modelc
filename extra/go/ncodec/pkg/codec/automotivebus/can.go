// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package automotivebus

import (
	"fmt"
	"strconv"

	"github.com/boschglobal/dse.modelc/extra/go/ncodec/internal/schema/AutomotiveBus/Stream/Frame"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/message"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/trace"
	flatbuffers "github.com/google/flatbuffers/go"
)

type CanCodec[T message.MessageType] struct {
	ModelName      string
	SimulationTime *float64
	MimeMap        map[string]*string
	builder        *flatbuffers.Builder
	frames         []flatbuffers.UOffsetT
	stream         *[]byte
	trace          trace.Trace[T]
}

func (c *CanCodec[T]) Configure(MimeType string, stream *[]byte, t trace.Trace[T]) error {
	if stream == nil {
		return fmt.Errorf("no stream provided")
	}
	var err error
	c.stream = stream
	c.builder = flatbuffers.NewBuilder(1024)

	// Validate and parse the MimeType
	c.MimeMap, err = DecodeMimeType(MimeType)
	if err != nil {
		return err
	}

	// Initialize the frames slice to store FlatBuffers frame offsets
	c.frames = make([]flatbuffers.UOffsetT, 0)

	if _t, ok := t.(*trace.CanTraceData[T]); ok {
		c.trace = _t
		_t.ModelInstName = c.ModelName
		_t.SimulationTime = c.SimulationTime
		_t.MimeMap = &c.MimeMap
	}

	return nil
}

func (c *CanCodec[T]) Trace(t trace.Trace[T]) error {
	c.trace = t
	return nil
}

func (c *CanCodec[T]) getStreamFromBuffer() ([]*Frame.CanFrame, error) {
	var list []*Frame.CanFrame
	_stream := Frame.GetSizePrefixedRootAsStream(*c.stream, 0)
	for i := range _stream.FramesLength() {
		frame := new(Frame.Frame)
		if _stream.Frames(frame, i) {
			switch frame.FType() {
			case Frame.FrameTypesCanFrame:
				canFrame := new(Frame.CanFrame)
				canFrame.Init(frame.Table().Bytes, frame.Table().Pos)
				list = append(list, canFrame)
			case Frame.FrameTypesNONE:
				fmt.Printf("Found NONE frame type, skipping...\n")
			default:
				return nil, fmt.Errorf("unknown frame type: %s", frame.FType().String())
			}
		} else {
			break
		}
	}

	return list, nil
}

func (c *CanCodec[T]) Read() (msg []*T, err error) {
	var msgs []*message.CanMessage
	list, _ := c.getStreamFromBuffer()

	for i := range list {
		var Node_id int64 = 0
		if _node_id, _ := c.Stat("Node_id"); _node_id != nil {
			Node_id, _ = strconv.ParseInt(*_node_id, 10, 32)
		}
		if Node_id != 0 && list[i].FrameId() == uint32(Node_id) {
			continue
		}
		msg := new(message.CanMessage)
		msg.Frame_id = list[i].FrameId()
		msg.Frame_type = list[i].FrameType()
		msg.Payload = list[i].PayloadBytes()
		msg.Sender = &message.CanSender{
			Bus_id:       list[i].BusId(),
			Node_id:      list[i].NodeId(),
			Interface_id: list[i].InterfaceId(),
		}

		if c.trace != nil {
			c.trace.TraceRX(any(*msg).(T))
		}

		msgs = append(msgs, msg)
	}
	return any(msgs).([]*T), nil
}

func (c *CanCodec[T]) Write(msg []*T) error {
	if msg == nil {
		return fmt.Errorf("no can message provided")
	}

	for _, msg := range msg {
		_msg := any(*msg).(message.CanMessage)
		payload := c.builder.CreateByteVector(_msg.Payload)
		Frame.CanFrameStart(c.builder)
		Frame.CanFrameAddFrameId(c.builder, _msg.Frame_id)
		Frame.CanFrameAddFrameType(c.builder, _msg.Frame_type)
		Frame.CanFrameAddPayload(c.builder, payload)
		Frame.CanFrameAddBusId(c.builder, _msg.Sender.Bus_id)
		Frame.CanFrameAddNodeId(c.builder, _msg.Sender.Node_id)
		Frame.CanFrameAddInterfaceId(c.builder, _msg.Sender.Interface_id)
		frame := Frame.CanFrameEnd(c.builder)

		if c.trace != nil {
			c.trace.TraceTX(any(*msg).(T))
		}

		c.frames = append(c.frames, frame)
	}
	return nil
}

func (c *CanCodec[T]) streamFinalize() []byte {
	Frame.StreamStartFramesVector(c.builder, len(c.frames))
	for i := len(c.frames) - 1; i >= 0; i-- {
		c.builder.PrependUOffsetT(c.frames[i])
	}
	frameVec := c.builder.EndVector(len(c.frames))

	Frame.StreamStart(c.builder)
	Frame.StreamAddFrames(c.builder, frameVec)
	stream := Frame.StreamEnd(c.builder)
	c.builder.FinishSizePrefixed(stream)
	buf := c.builder.FinishedBytes()
	return buf
}

func (c *CanCodec[T]) Flush() error {
	buf := c.streamFinalize()
	c.builder.Reset()
	if len(buf) == 0 {
		return nil
	}
	*c.stream = buf
	return nil
}

func (c *CanCodec[T]) Truncate() error {
	c.builder.Reset()
	*c.stream = make([]byte, 0)
	c.frames = make([]flatbuffers.UOffsetT, 0)
	return nil
}

func (c *CanCodec[T]) Stat(param string) (*string, error) {
	if _param := c.MimeMap[param]; _param != nil {
		return _param, nil
	}
	return nil, fmt.Errorf("parameter %s not found in Can codec", param)
}
