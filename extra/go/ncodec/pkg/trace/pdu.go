package trace

import (
	"fmt"

	"github.com/boschglobal/dse.modelc/extra/go/ncodec/internal/schema/AutomotiveBus/Stream/Pdu"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/message"
)

type PduTraceData[T message.MessageType] struct {
	ModelInstName  string
	SimulationTime *float64
	Identifier     string
	Wildcard       bool
	Filter         map[uint32]bool
	MimeMap        *map[string]*string
}

func (t *PduTraceData[T]) TraceRX(msg T) {
	t.tracePdu("RX", any(msg).(message.PduMessage))
}

func (t *PduTraceData[T]) TraceTX(msg T) {
	t.tracePdu("TX", any(msg).(message.PduMessage))
}

func (t *PduTraceData[T]) tracePdu(direction string, pdu message.PduMessage) error {

	if t.Identifier == "" {
		swc_id := ""
		ecu_id := ""
		if _swc_id := (*t.MimeMap)["swc_id"]; _swc_id != nil {
			swc_id = *_swc_id
		}
		if _ecu_id := (*t.MimeMap)["ecu_id"]; _ecu_id != nil {
			ecu_id = *_ecu_id
		}
		t.Identifier = fmt.Sprintf("%s:%s", swc_id, ecu_id)
	}
	if !t.Wildcard {
		if !t.Filter[pdu.Id] {
			return nil
		}
	}
	var identifier string
	_len := len(pdu.Payload)
	var b string
	if _len <= 16 {
		for _, byte := range pdu.Payload {
			b += fmt.Sprintf("%02x ", byte)
		}
	} else {
		for i, byte := range pdu.Payload {
			if i%32 == 0 {
				b += " "
			}
			if i%8 == 0 {
				b += " "
			}
			b += fmt.Sprintf("%02x ", byte)
		}
	}

	if direction == "RX" {
		identifier = fmt.Sprintf("%d:%d", pdu.Swc_id, pdu.Ecu_id)
	} else {
		identifier = t.Identifier
	}
	fmt.Printf("(%s) %.6f [%s] %s %02x %d :%s", t.ModelInstName,
		*t.SimulationTime, identifier, direction, pdu.Id, _len, b)

	// Transport
	switch pdu.Type {
	case Pdu.TransportMetadataCan:
		{
			fmt.Printf("    CAN:    frame_format=%d  frame_type=%d  interface_id=%d  network_id=%d",
				pdu.CanMetadata.Format, pdu.CanMetadata.Type,
				pdu.CanMetadata.Interface_id, pdu.CanMetadata.Network_id)
		}
	case Pdu.TransportMetadataIp:
		{
			// ETH
			fmt.Printf("    ETH:    src_mac=%016x  dst_mac=%016x",
				pdu.IpMetadata.Eth_src_mac,
				pdu.IpMetadata.Eth_dst_mac)
			fmt.Printf(
				"    ETH:    ethertype=%04x  tci_pcp=%02x  tci_dei=%02x  tci_vid=%04x",
				pdu.IpMetadata.Eth_ethertype,
				pdu.IpMetadata.Eth_tci_pcp,
				pdu.IpMetadata.Eth_tci_dei,
				pdu.IpMetadata.Eth_tci_vid)

			// IP
			switch pdu.IpMetadata.Ip_addr_type {
			case Pdu.IpAddrv4:
				{
					fmt.Printf("    IP:     src_addr=%08x  dst_addr=%08x",
						pdu.IpMetadata.Ip_addr.Ip_v4.Src_ip,
						pdu.IpMetadata.Ip_addr.Ip_v4.Dst_ip)
				}
			case Pdu.IpAddrv6:
				{
					fmt.Printf(
						"    IP:     src_addr=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
						pdu.IpMetadata.Ip_addr.Ip_v6.Src_ip[0],
						pdu.IpMetadata.Ip_addr.Ip_v6.Src_ip[1],
						pdu.IpMetadata.Ip_addr.Ip_v6.Src_ip[2],
						pdu.IpMetadata.Ip_addr.Ip_v6.Src_ip[3],
						pdu.IpMetadata.Ip_addr.Ip_v6.Src_ip[4],
						pdu.IpMetadata.Ip_addr.Ip_v6.Src_ip[5],
						pdu.IpMetadata.Ip_addr.Ip_v6.Src_ip[6],
						pdu.IpMetadata.Ip_addr.Ip_v6.Src_ip[7])
					fmt.Printf(
						"    IP:     dst_addr=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
						pdu.IpMetadata.Ip_addr.Ip_v6.Dst_ip[0],
						pdu.IpMetadata.Ip_addr.Ip_v6.Dst_ip[1],
						pdu.IpMetadata.Ip_addr.Ip_v6.Dst_ip[2],
						pdu.IpMetadata.Ip_addr.Ip_v6.Dst_ip[3],
						pdu.IpMetadata.Ip_addr.Ip_v6.Dst_ip[4],
						pdu.IpMetadata.Ip_addr.Ip_v6.Dst_ip[5],
						pdu.IpMetadata.Ip_addr.Ip_v6.Dst_ip[6],
						pdu.IpMetadata.Ip_addr.Ip_v6.Dst_ip[7])
				}
			}
			fmt.Printf("    IP:     src_port=%04x  dst_port=%04x  proto=%d",
				pdu.IpMetadata.Ip_src_port,
				pdu.IpMetadata.Ip_dst_port,
				pdu.IpMetadata.Ip_protocol)

			// Socket Adapter
			switch pdu.IpMetadata.So_ad_type {
			case Pdu.SocketAdapterdo_ip:
				{
					fmt.Printf("    DOIP:   protocol_version=%d  payload_type=%d",
						pdu.IpMetadata.So_ad.Do_ip.Protocol_version,
						pdu.IpMetadata.So_ad.Do_ip.Payload_type)
				}
			case Pdu.SocketAdaptersome_ip:
				{
					fmt.Printf("    SOMEIP: protocol_version=%d  interface_version=%d",
						pdu.IpMetadata.So_ad.Some_ip.Protocol_version,
						pdu.IpMetadata.So_ad.Some_ip.Interface_version)
					fmt.Printf("    SOMEIP: request_id=%d  return_code=%d",
						pdu.IpMetadata.So_ad.Some_ip.Request_id,
						pdu.IpMetadata.So_ad.Some_ip.Return_code)
					fmt.Printf("    SOMEIP: message_type=%d  message_id=%d  length=%d",
						pdu.IpMetadata.So_ad.Some_ip.Message_type,
						pdu.IpMetadata.So_ad.Some_ip.Message_id,
						pdu.IpMetadata.So_ad.Some_ip.Length)
				}
			}

		}
	}
	return nil
}
