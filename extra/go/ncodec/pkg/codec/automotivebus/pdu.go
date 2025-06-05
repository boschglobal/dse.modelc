// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package automotivebus

import (
	"fmt"
	"strconv"

	"github.com/boschglobal/dse.modelc/extra/go/ncodec/internal/schema/AutomotiveBus/Stream/Pdu"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/message"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/trace"
	flatbuffers "github.com/google/flatbuffers/go"
)

type PduCodec[T message.MessageType] struct {
	ModelName      string
	SimulationTime *float64
	MimeMap        map[string]*string
	builder        *flatbuffers.Builder
	pdus           []flatbuffers.UOffsetT
	stream         *[]byte
	trace          trace.Trace[T]
}

func (p *PduCodec[T]) Configure(MimeType string, stream *[]byte, t trace.Trace[T]) error {
	if stream == nil {
		return fmt.Errorf("no stream provided")
	}
	var err error
	p.stream = stream
	p.builder = flatbuffers.NewBuilder(1024)

	// Validate and parse the MimeType
	p.MimeMap, err = DecodeMimeType(MimeType)
	if err != nil {
		return fmt.Errorf("failed to parse or validate MimeType in PDU codec: %s", err)
	}

	p.pdus = make([]flatbuffers.UOffsetT, 0)

	if _t, ok := t.(*trace.PduTraceData[T]); ok {
		p.trace = _t
		_t.ModelInstName = p.ModelName
		_t.SimulationTime = p.SimulationTime
		_t.MimeMap = &p.MimeMap
	}

	return nil
}

func (p *PduCodec[T]) Trace(t trace.Trace[T]) error {
	p.trace = t
	return nil
}

func (p *PduCodec[T]) emitCanMessageMetadata(msg message.PduMessage) (flatbuffers.UOffsetT, error) {
	if msg.CanMetadata == nil {
		return 0, fmt.Errorf("no CanMessageMetadata provided")
	}

	Pdu.CanMessageMetadataStart(p.builder)
	Pdu.CanMessageMetadataAddMessageFormat(p.builder, msg.CanMetadata.Format)
	Pdu.CanMessageMetadataAddFrameType(p.builder, msg.CanMetadata.Type)
	Pdu.CanMessageMetadataAddInterfaceId(p.builder, msg.CanMetadata.Interface_id)
	Pdu.CanMessageMetadataAddNetworkId(p.builder, msg.CanMetadata.Network_id)
	return Pdu.CanMessageMetadataEnd(p.builder), nil
}

func (p *PduCodec[T]) emitIpAddrV4(msg message.PduMessage) flatbuffers.UOffsetT {
	addr := msg.IpMetadata.Ip_addr.Ip_v4
	Pdu.IpV4Start(p.builder)
	Pdu.IpV4AddSrcAddr(p.builder, addr.Src_ip)
	Pdu.IpV4AddDstAddr(p.builder, addr.Dst_ip)
	return Pdu.IpV4End(p.builder)
}

func (p *PduCodec[T]) emitIpAddrV6(msg message.PduMessage) flatbuffers.UOffsetT {
	addr := msg.IpMetadata.Ip_addr.Ip_v6

	Pdu.IpV6Start(p.builder)
	src_addr := Pdu.CreateIpAddressV6(p.builder, addr.Src_ip[0],
		addr.Src_ip[1], addr.Src_ip[2],
		addr.Src_ip[3], addr.Src_ip[4],
		addr.Src_ip[5], addr.Src_ip[6],
		addr.Src_ip[7])
	Pdu.IpV6AddSrcAddr(p.builder, src_addr)
	dst_addr := Pdu.CreateIpAddressV6(p.builder, addr.Dst_ip[0],
		addr.Dst_ip[1], addr.Dst_ip[2],
		addr.Dst_ip[3], addr.Dst_ip[4],
		addr.Dst_ip[5], addr.Dst_ip[6],
		addr.Dst_ip[7])
	Pdu.IpV6AddDstAddr(p.builder, dst_addr)
	return Pdu.IpV6End(p.builder)
}

func (p *PduCodec[T]) emitDoIp(msg message.PduMessage) flatbuffers.UOffsetT {
	a := msg.IpMetadata.So_ad.Do_ip
	Pdu.DoIpMetadataStart(p.builder)
	Pdu.DoIpMetadataAddProtocolVersion(p.builder, a.Protocol_version)
	Pdu.DoIpMetadataAddPayloadType(p.builder, a.Payload_type)
	return Pdu.DoIpMetadataEnd(p.builder)
}

func (p *PduCodec[T]) emitSomeIp(msg message.PduMessage) flatbuffers.UOffsetT {
	a := msg.IpMetadata.So_ad.Some_ip
	Pdu.SomeIpMetadataStart(p.builder)
	Pdu.SomeIpMetadataAddMessageId(p.builder, a.Message_id)
	Pdu.SomeIpMetadataAddLength(p.builder, a.Length)
	Pdu.SomeIpMetadataAddRequestId(p.builder, a.Request_id)
	Pdu.SomeIpMetadataAddProtocolVersion(p.builder, a.Protocol_version)
	Pdu.SomeIpMetadataAddInterfaceVersion(p.builder, a.Interface_version)
	Pdu.SomeIpMetadataAddMessageType(p.builder, a.Message_type)
	Pdu.SomeIpMetadataAddReturnCode(p.builder, a.Return_code)
	return Pdu.SomeIpMetadataEnd(p.builder)
}

func (p *PduCodec[T]) emitIpMessageMetdata(msg message.PduMessage) (flatbuffers.UOffsetT, error) {
	if msg.IpMetadata == nil {
		return 0, fmt.Errorf("no IpMessageMetadata provided")
	}

	var addr_type flatbuffers.UOffsetT
	switch msg.IpMetadata.Ip_addr_type {
	case Pdu.IpAddrv4:
		{
			addr_type = p.emitIpAddrV4(msg)
		}
	case Pdu.IpAddrv6:
		{
			addr_type = p.emitIpAddrV6(msg)
		}
	case Pdu.IpAddrNONE:
		{
			// NOP
		}
	default:
		return 0, fmt.Errorf("no IP address type provided")
	}

	var socket_adapter flatbuffers.UOffsetT
	switch msg.IpMetadata.So_ad_type {
	case Pdu.SocketAdapterdo_ip:
		{
			socket_adapter = p.emitDoIp(msg)
		}
	case Pdu.SocketAdaptersome_ip:
		{
			socket_adapter = p.emitSomeIp(msg)
		}
	case Pdu.SocketAdapterNONE:
		{
			// NOP
		}
	default:
		return 0, fmt.Errorf("no SocketAdapter type provided")
	}

	Pdu.IpMessageMetadataStart(p.builder)
	Pdu.IpMessageMetadataAddEthDstMac(p.builder, msg.IpMetadata.Eth_dst_mac)
	Pdu.IpMessageMetadataAddEthSrcMac(p.builder, msg.IpMetadata.Eth_src_mac)
	Pdu.IpMessageMetadataAddEthEthertype(p.builder, msg.IpMetadata.Eth_ethertype)
	Pdu.IpMessageMetadataAddEthTciPcp(p.builder, msg.IpMetadata.Eth_tci_pcp)
	Pdu.IpMessageMetadataAddEthTciDei(p.builder, msg.IpMetadata.Eth_tci_dei)
	Pdu.IpMessageMetadataAddEthTciVid(p.builder, msg.IpMetadata.Eth_tci_vid)
	Pdu.IpMessageMetadataAddIpAddrType(p.builder, msg.IpMetadata.Ip_addr_type)
	Pdu.IpMessageMetadataAddIpAddr(p.builder, addr_type)
	Pdu.IpMessageMetadataAddIpProtocol(p.builder, msg.IpMetadata.Ip_protocol)
	Pdu.IpMessageMetadataAddIpSrcPort(p.builder, msg.IpMetadata.Ip_src_port)
	Pdu.IpMessageMetadataAddIpDstPort(p.builder, msg.IpMetadata.Ip_dst_port)
	Pdu.IpMessageMetadataAddAdapterType(p.builder, msg.IpMetadata.So_ad_type)
	Pdu.IpMessageMetadataAddAdapter(p.builder, socket_adapter)

	return Pdu.IpMessageMetadataEnd(p.builder), nil
}

func (p *PduCodec[T]) Write(msg []*T) error {
	if msg == nil {
		return fmt.Errorf("no pdu provided")
	}

	for _, msg := range msg {
		var transport flatbuffers.UOffsetT
		var err error
		_msg := any(*msg).(message.PduMessage)
		switch _msg.Type {
		case Pdu.TransportMetadataCan:
			{
				transport, err = p.emitCanMessageMetadata(_msg)
				if err != nil {
					return err
				}
			}
		case Pdu.TransportMetadataIp:
			{
				transport, err = p.emitIpMessageMetdata(_msg)
				if err != nil {
					return err
				}
			}
		case Pdu.TransportMetadataNONE:
			{
				// NOP
			}
		default:
			return fmt.Errorf("no Message Metadata provided")
		}
		if err != nil {
			return err
		}
		payload := p.builder.CreateByteVector(_msg.Payload)

		Pdu.PduStart(p.builder)
		Pdu.PduAddId(p.builder, _msg.Id)
		Pdu.PduAddPayload(p.builder, payload)
		Pdu.PduAddTransportType(p.builder, _msg.Type)
		Pdu.PduAddTransport(p.builder, transport)

		if _msg.Swc_id == 0 {
			if swc_id, _ := p.Stat("swc_id"); swc_id != nil {
				i, err := strconv.ParseInt(*swc_id, 0, 32)
				if err != nil && i != 0 {
					_msg.Swc_id = uint32(i)
				}
			}
		}
		Pdu.PduAddSwcId(p.builder, _msg.Swc_id)

		if _msg.Ecu_id == 0 {
			if ecu_id, _ := p.Stat("ecu_id"); ecu_id != nil {
				i, err := strconv.ParseInt(*ecu_id, 0, 32)
				if err != nil && i != 0 {
					_msg.Ecu_id = uint32(i)
				}
			}
		}

		Pdu.PduAddEcuId(p.builder, _msg.Ecu_id)
		pdu := Pdu.PduEnd(p.builder)

		if p.trace != nil {
			p.trace.TraceTX(any(*msg).(T))
		}

		p.pdus = append(p.pdus, pdu)

	}

	return nil
}

func (p *PduCodec[T]) decodeIpAddr(transport *Pdu.IpMessageMetadata, Ip_metadata *message.NCodecIpCanMessageMetadata) {
	Ip_table := new(flatbuffers.Table)
	if transport.IpAddr(Ip_table) {
		Ip_metadata.Ip_addr_type = transport.IpAddrType()
		switch Ip_metadata.Ip_addr_type {
		case Pdu.IpAddrv4:
			{
				Ip_v4 := new(Pdu.IpV4)
				Ip_v4.Init(Ip_table.Bytes, Ip_table.Pos)
				addr := &Ip_metadata.Ip_addr.Ip_v4
				addr.Src_ip = Ip_v4.SrcAddr()
				addr.Dst_ip = Ip_v4.DstAddr()
			}
		case Pdu.IpAddrv6:
			{
				Ip_v6 := new(Pdu.IpV6)
				Ip_v6.Init(Ip_table.Bytes, Ip_table.Pos)
				addr := &Ip_metadata.Ip_addr.Ip_v6

				SrcAddr := new(Pdu.IpAddressV6)
				Ip_v6.SrcAddr(SrcAddr)
				addr.Src_ip[0] = SrcAddr.V0()
				addr.Src_ip[1] = SrcAddr.V1()
				addr.Src_ip[2] = SrcAddr.V2()
				addr.Src_ip[3] = SrcAddr.V3()
				addr.Src_ip[4] = SrcAddr.V4()
				addr.Src_ip[5] = SrcAddr.V5()
				addr.Src_ip[6] = SrcAddr.V6()
				addr.Src_ip[7] = SrcAddr.V7()
				DstAddr := new(Pdu.IpAddressV6)
				Ip_v6.DstAddr(DstAddr)
				addr.Dst_ip[0] = DstAddr.V0()
				addr.Dst_ip[1] = DstAddr.V1()
				addr.Dst_ip[2] = DstAddr.V2()
				addr.Dst_ip[3] = DstAddr.V3()
				addr.Dst_ip[4] = DstAddr.V4()
				addr.Dst_ip[5] = DstAddr.V5()
				addr.Dst_ip[6] = DstAddr.V6()
				addr.Dst_ip[7] = DstAddr.V7()
			}
		}
	}
}

func (p *PduCodec[T]) decodeSoAd(transport *Pdu.IpMessageMetadata, Ip_metadata *message.NCodecIpCanMessageMetadata) {
	ad_table := new(flatbuffers.Table)
	if transport.Adapter(ad_table) {
		Ip_metadata.So_ad_type = transport.AdapterType()
		switch Ip_metadata.So_ad_type {
		case Pdu.SocketAdapterdo_ip:
			{
				do_ip := new(Pdu.DoIpMetadata)
				do_ip.Init(ad_table.Bytes, ad_table.Pos)
				So_ad := &Ip_metadata.So_ad.Do_ip
				So_ad.Protocol_version = do_ip.ProtocolVersion()
				So_ad.Payload_type = do_ip.PayloadType()
			}
		case Pdu.SocketAdaptersome_ip:
			{
				some_ip := new(Pdu.SomeIpMetadata)
				some_ip.Init(ad_table.Bytes, ad_table.Pos)
				So_ad := &Ip_metadata.So_ad.Some_ip
				So_ad.Message_id = some_ip.MessageId()
				So_ad.Length = some_ip.Length()
				So_ad.Request_id = some_ip.RequestId()
				So_ad.Protocol_version = some_ip.ProtocolVersion()
				So_ad.Interface_version = some_ip.InterfaceVersion()
				So_ad.Message_type = some_ip.MessageType()
				So_ad.Return_code = some_ip.ReturnCode()
			}
		}
	}
}

func (p *PduCodec[T]) decodeTransport(msg *message.PduMessage, pdu *Pdu.Pdu) {
	table := new(flatbuffers.Table)
	if pdu.Transport(table) {
		switch pdu.TransportType() {
		case Pdu.TransportMetadataCan:
			{
				transport := new(Pdu.CanMessageMetadata)
				transport.Init(table.Bytes, table.Pos)
				can_metadata := new(message.NCodecPduCanMessageMetadata)
				can_metadata.Format = transport.MessageFormat()
				can_metadata.Type = transport.FrameType()
				can_metadata.Interface_id = transport.InterfaceId()
				can_metadata.Network_id = transport.NetworkId()
				msg.CanMetadata = can_metadata
			}
		case Pdu.TransportMetadataIp:
			{
				transport := new(Pdu.IpMessageMetadata)
				transport.Init(table.Bytes, table.Pos)
				Ip_metadata := new(message.NCodecIpCanMessageMetadata)
				Ip_metadata.Eth_dst_mac = transport.EthDstMac()
				Ip_metadata.Eth_src_mac = transport.EthSrcMac()
				Ip_metadata.Eth_ethertype = transport.EthEthertype()
				Ip_metadata.Eth_tci_pcp = transport.EthTciPcp()
				Ip_metadata.Eth_tci_dei = transport.EthTciDei()
				Ip_metadata.Eth_tci_vid = transport.EthTciVid()
				Ip_metadata.Ip_protocol = transport.IpProtocol()
				Ip_metadata.Ip_src_port = transport.IpSrcPort()
				Ip_metadata.Ip_dst_port = transport.IpDstPort()

				p.decodeIpAddr(transport, Ip_metadata)
				p.decodeSoAd(transport, Ip_metadata)

				msg.IpMetadata = Ip_metadata
			}
		default:
			{
				//NOP
			}
		}
	}
}

func (p *PduCodec[T]) getStreamFromBuffer() ([]*Pdu.Pdu, error) {
	var list []*Pdu.Pdu
	_stream := Pdu.GetSizePrefixedRootAsStream(*p.stream, 0)
	for i := range _stream.PdusLength() {
		obj := new(Pdu.Pdu)
		if _stream.Pdus(obj, i) {
			list = append(list, obj)
		} else {
			break
		}
	}

	return list, nil
}

func (p *PduCodec[T]) Read() ([]*T, error) {
	var msgs []*message.PduMessage
	list, _ := p.getStreamFromBuffer()
	for i := range list {
		var swc_id int64 = 0
		if _swc_id, _ := p.Stat("swc_id"); _swc_id != nil {
			swc_id, _ = strconv.ParseInt(*_swc_id, 10, 32)
		}
		if swc_id != 0 && list[i].SwcId() == uint32(swc_id) {
			continue
		}
		msg := new(message.PduMessage)
		msg.Id = list[i].Id()
		msg.Ecu_id = list[i].EcuId()
		msg.Swc_id = list[i].SwcId()
		msg.Type = list[i].TransportType()
		msg.Payload = list[i].PayloadBytes()

		p.decodeTransport(msg, list[i])

		if p.trace != nil {
			p.trace.TraceRX(any(*msg).(T))
		}

		msgs = append(msgs, msg)
	}
	return any(msgs).([]*T), nil
}

func (p *PduCodec[T]) streamFinalize() []byte {
	Pdu.StreamStartPdusVector(p.builder, len(p.pdus))
	for i := len(p.pdus) - 1; i >= 0; i-- {
		p.builder.PrependUOffsetT(p.pdus[i])
	}
	pdusVec := p.builder.EndVector(len(p.pdus))

	Pdu.StreamStart(p.builder)
	Pdu.StreamAddPdus(p.builder, pdusVec)
	stream := Pdu.StreamEnd(p.builder)
	p.builder.FinishSizePrefixed(stream)
	buf := p.builder.FinishedBytes()
	return buf
}

func (p *PduCodec[T]) Flush() error {
	buf := p.streamFinalize()
	p.builder.Reset()
	if len(buf) == 0 {
		return nil
	}
	*p.stream = buf
	return nil
}

func (p *PduCodec[T]) Truncate() error {
	p.builder.Reset()
	*p.stream = make([]byte, 0)
	p.pdus = make([]flatbuffers.UOffsetT, 0)
	return nil
}

func (p *PduCodec[T]) Stat(param string) (*string, error) {
	if _param := p.MimeMap[param]; _param != nil {
		return _param, nil
	}
	return nil, fmt.Errorf("parameter %s not found in PDU codec", param)
}
