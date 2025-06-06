// Code generated by the FlatBuffers compiler. DO NOT EDIT.

package Ethernet

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type RegisterFile struct {
	_tab flatbuffers.Table
}

func GetRootAsRegisterFile(buf []byte, offset flatbuffers.UOffsetT) *RegisterFile {
	n := flatbuffers.GetUOffsetT(buf[offset:])
	x := &RegisterFile{}
	x.Init(buf, n+offset)
	return x
}

func GetSizePrefixedRootAsRegisterFile(buf []byte, offset flatbuffers.UOffsetT) *RegisterFile {
	n := flatbuffers.GetUOffsetT(buf[offset+flatbuffers.SizeUint32:])
	x := &RegisterFile{}
	x.Init(buf, n+offset+flatbuffers.SizeUint32)
	return x
}

func (rcv *RegisterFile) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *RegisterFile) Table() flatbuffers.Table {
	return rcv._tab
}

func (rcv *RegisterFile) Buffer(obj *MetaFrame, j int) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		x := rcv._tab.Vector(o)
		x += flatbuffers.UOffsetT(j) * 4
		x = rcv._tab.Indirect(x)
		obj.Init(rcv._tab.Bytes, x)
		return true
	}
	return false
}

func (rcv *RegisterFile) BufferLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func RegisterFileStart(builder *flatbuffers.Builder) {
	builder.StartObject(1)
}
func RegisterFileAddBuffer(builder *flatbuffers.Builder, buffer flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(0, flatbuffers.UOffsetT(buffer), 0)
}
func RegisterFileStartBufferVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(4, numElems, 4)
}
func RegisterFileEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
	return builder.EndObject()
}
