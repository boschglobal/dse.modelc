// Code generated by the FlatBuffers compiler. DO NOT EDIT.

package Channel

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type SignalLookup struct {
	_tab flatbuffers.Table
}

func GetRootAsSignalLookup(buf []byte, offset flatbuffers.UOffsetT) *SignalLookup {
	n := flatbuffers.GetUOffsetT(buf[offset:])
	x := &SignalLookup{}
	x.Init(buf, n+offset)
	return x
}

func GetSizePrefixedRootAsSignalLookup(buf []byte, offset flatbuffers.UOffsetT) *SignalLookup {
	n := flatbuffers.GetUOffsetT(buf[offset+flatbuffers.SizeUint32:])
	x := &SignalLookup{}
	x.Init(buf, n+offset+flatbuffers.SizeUint32)
	return x
}

func (rcv *SignalLookup) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *SignalLookup) Table() flatbuffers.Table {
	return rcv._tab
}

func (rcv *SignalLookup) SignalUid() uint32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		return rcv._tab.GetUint32(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *SignalLookup) MutateSignalUid(n uint32) bool {
	return rcv._tab.MutateUint32Slot(4, n)
}

func (rcv *SignalLookup) Name() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(6))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func SignalLookupStart(builder *flatbuffers.Builder) {
	builder.StartObject(2)
}
func SignalLookupAddSignalUid(builder *flatbuffers.Builder, signalUid uint32) {
	builder.PrependUint32Slot(0, signalUid, 0)
}
func SignalLookupAddName(builder *flatbuffers.Builder, name flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(1, flatbuffers.UOffsetT(name), 0)
}
func SignalLookupEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
	return builder.EndObject()
}
