// Code generated by the FlatBuffers compiler. DO NOT EDIT.

package Pdu

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type IpV6 struct {
	_tab flatbuffers.Table
}

func GetRootAsIpV6(buf []byte, offset flatbuffers.UOffsetT) *IpV6 {
	n := flatbuffers.GetUOffsetT(buf[offset:])
	x := &IpV6{}
	x.Init(buf, n+offset)
	return x
}

func GetSizePrefixedRootAsIpV6(buf []byte, offset flatbuffers.UOffsetT) *IpV6 {
	n := flatbuffers.GetUOffsetT(buf[offset+flatbuffers.SizeUint32:])
	x := &IpV6{}
	x.Init(buf, n+offset+flatbuffers.SizeUint32)
	return x
}

func (rcv *IpV6) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *IpV6) Table() flatbuffers.Table {
	return rcv._tab
}

func (rcv *IpV6) SrcAddr(obj *IpAddressV6) *IpAddressV6 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		x := o + rcv._tab.Pos
		if obj == nil {
			obj = new(IpAddressV6)
		}
		obj.Init(rcv._tab.Bytes, x)
		return obj
	}
	return nil
}

func (rcv *IpV6) DstAddr(obj *IpAddressV6) *IpAddressV6 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(6))
	if o != 0 {
		x := o + rcv._tab.Pos
		if obj == nil {
			obj = new(IpAddressV6)
		}
		obj.Init(rcv._tab.Bytes, x)
		return obj
	}
	return nil
}

func IpV6Start(builder *flatbuffers.Builder) {
	builder.StartObject(2)
}
func IpV6AddSrcAddr(builder *flatbuffers.Builder, srcAddr flatbuffers.UOffsetT) {
	builder.PrependStructSlot(0, flatbuffers.UOffsetT(srcAddr), 0)
}
func IpV6AddDstAddr(builder *flatbuffers.Builder, dstAddr flatbuffers.UOffsetT) {
	builder.PrependStructSlot(1, flatbuffers.UOffsetT(dstAddr), 0)
}
func IpV6End(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
	return builder.EndObject()
}
