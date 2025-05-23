// Code generated by the FlatBuffers compiler. DO NOT EDIT.

package channel

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

/// ModelRegister: Register a Model with the SimBus.
/// (Model -> SimBus)
type ModelRegister struct {
	_tab flatbuffers.Table
}

func GetRootAsModelRegister(buf []byte, offset flatbuffers.UOffsetT) *ModelRegister {
	n := flatbuffers.GetUOffsetT(buf[offset:])
	x := &ModelRegister{}
	x.Init(buf, n+offset)
	return x
}

func GetSizePrefixedRootAsModelRegister(buf []byte, offset flatbuffers.UOffsetT) *ModelRegister {
	n := flatbuffers.GetUOffsetT(buf[offset+flatbuffers.SizeUint32:])
	x := &ModelRegister{}
	x.Init(buf, n+offset+flatbuffers.SizeUint32)
	return x
}

func (rcv *ModelRegister) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *ModelRegister) Table() flatbuffers.Table {
	return rcv._tab
}

/// Specify the step_size of the Model.
func (rcv *ModelRegister) StepSize() float64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		return rcv._tab.GetFloat64(o + rcv._tab.Pos)
	}
	return 0.0
}

/// Specify the step_size of the Model.
func (rcv *ModelRegister) MutateStepSize(n float64) bool {
	return rcv._tab.MutateFloat64Slot(4, n)
}

/// Model UID of the Model sending this message.
func (rcv *ModelRegister) ModelUid() uint32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(6))
	if o != 0 {
		return rcv._tab.GetUint32(o + rcv._tab.Pos)
	}
	return 0
}

/// Model UID of the Model sending this message.
func (rcv *ModelRegister) MutateModelUid(n uint32) bool {
	return rcv._tab.MutateUint32Slot(6, n)
}

/// Notify UID for sending consolidated Notify messages to a single endpoint
/// which represents several Model UIDs.
func (rcv *ModelRegister) NotifyUid() uint32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(8))
	if o != 0 {
		return rcv._tab.GetUint32(o + rcv._tab.Pos)
	}
	return 0
}

/// Notify UID for sending consolidated Notify messages to a single endpoint
/// which represents several Model UIDs.
func (rcv *ModelRegister) MutateNotifyUid(n uint32) bool {
	return rcv._tab.MutateUint32Slot(8, n)
}

func ModelRegisterStart(builder *flatbuffers.Builder) {
	builder.StartObject(3)
}
func ModelRegisterAddStepSize(builder *flatbuffers.Builder, stepSize float64) {
	builder.PrependFloat64Slot(0, stepSize, 0.0)
}
func ModelRegisterAddModelUid(builder *flatbuffers.Builder, modelUid uint32) {
	builder.PrependUint32Slot(1, modelUid, 0)
}
func ModelRegisterAddNotifyUid(builder *flatbuffers.Builder, notifyUid uint32) {
	builder.PrependUint32Slot(2, notifyUid, 0)
}
func ModelRegisterEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
	return builder.EndObject()
}
