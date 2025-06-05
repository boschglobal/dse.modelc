package message

import (
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/internal/schema/AutomotiveBus/Stream/Pdu"
)

type PduMessage struct {
	Id          uint32
	Payload     []byte
	Swc_id      uint32
	Ecu_id      uint32
	Type        Pdu.TransportMetadata
	CanMetadata *NCodecPduCanMessageMetadata
	IpMetadata  *NCodecIpCanMessageMetadata
}

type NCodecPduCanMessageMetadata struct {
	Format       Pdu.CanMessageFormat
	Type         Pdu.CanFrameType
	Interface_id uint32
	Network_id   uint32
}

type NCodecPduIpAddrV4 struct {
	Src_ip uint32
	Dst_ip uint32
}

type NCodecPduIpAddrV6 struct {
	Src_ip [8]uint16
	Dst_ip [8]uint16
}

type NCodecIpAddr struct {
	Ip_v4 NCodecPduIpAddrV4
	Ip_v6 NCodecPduIpAddrV6
}

type NCodecPduDoIpAdapter struct {
	Protocol_version uint8
	Payload_type     uint16
}

type NCodecPduSomeIpAdapter struct {
	Message_id        uint32
	Length            uint32
	Request_id        uint32
	Protocol_version  uint8
	Interface_version uint8
	Message_type      uint8
	Return_code       uint8
}

type NCodecPduSocketAdapter struct {
	Do_ip   NCodecPduDoIpAdapter
	Some_ip NCodecPduSomeIpAdapter
}

type NCodecIpCanMessageMetadata struct {
	Eth_dst_mac   uint64
	Eth_src_mac   uint64
	Eth_ethertype uint16
	Eth_tci_pcp   uint8
	Eth_tci_dei   uint8
	Eth_tci_vid   uint16
	Ip_protocol   Pdu.IpProtocol
	Ip_addr_type  Pdu.IpAddr
	Ip_addr       NCodecIpAddr
	Ip_src_port   uint16
	Ip_dst_port   uint16
	So_ad_type    Pdu.SocketAdapter
	So_ad         NCodecPduSocketAdapter
}
