package automotivebus_test

import (
	"fmt"
	"testing"

	"github.com/boschglobal/dse.modelc/extra/go/ncodec/internal/schema/AutomotiveBus/Stream/Pdu"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/message"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/ncodec"
	"github.com/stretchr/testify/assert"
)

func TestPduCodec_Configure(t *testing.T) {
	tests := []struct {
		name      string
		mimeType  string
		want      map[string]string
		wantError string
	}{
		{
			name:      "empty string",
			mimeType:  "",
			wantError: "failed to parse or validate MimeType in PDU codec: MimeType is empty",
		},
		{
			name:      "single key-value",
			mimeType:  "interface=stream",
			wantError: "failed to parse or validate MimeType in PDU codec: missing required mimetype parameter",
		},
		{
			name:      "wrong type parameter",
			mimeType:  "interface=stream;type=qux;schema=fbs",
			wantError: "failed to parse or validate MimeType in PDU codec: unsupported type: qux",
		},
		{
			name:      "wrong interface parameter",
			mimeType:  "interface=qux;type=pdu;schema=fbs",
			wantError: "failed to parse or validate MimeType in PDU codec: wrong interface: qux",
		},
		{
			name:      "wrong schema parameter",
			mimeType:  "interface=stream;type=pdu;schema=qux",
			wantError: "failed to parse or validate MimeType in PDU codec: wrong schema: qux",
		},
		{
			name:     "correct required parameters",
			mimeType: "interface=stream;type=pdu;schema=fbs",
			want:     map[string]string{"interface": "stream", "type": "pdu", "schema": "fbs"},
		},
		{
			name:      "unexpected mimetype parameter",
			mimeType:  "interface=stream;type=pdu;schema=fbs;error=true",
			wantError: "failed to parse or validate MimeType in PDU codec: unexpected mimetype parameter: error",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			s := new([]byte)
			p, gotErr := ncodec.NewNetworkCodec[message.PduMessage](tc.mimeType, s, "test", nil)
			if tc.wantError == "" {
				assert.NoError(t, gotErr)
				for k, v := range tc.want {
					_v, _ := p.Stat(k)
					assert.Equal(t, v, *_v)
				}
				assert.NotNil(t, p)
			} else {
				assert.Error(t, gotErr)
				assert.Equal(t, fmt.Errorf("%s", tc.wantError), gotErr)
				assert.Nil(t, p)
			}
		})
	}
}

func TestPduCodec_Write(t *testing.T) {
	tests := []struct {
		name      string
		mimeType  string
		msg       []*message.PduMessage
		wantError string
	}{
		{
			name:      "invalid message",
			mimeType:  "interface=stream;type=pdu;schema=fbs",
			msg:       nil,
			wantError: "no pdu provided",
		},
		{
			name:     "valid no metadata message",
			mimeType: "interface=stream;type=pdu;schema=fbs;swc_id=1;ecu_id=2",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  1,
					Ecu_id:  2,
					Type:    Pdu.TransportMetadataNONE,
				},
			},
		},
		{
			name:     "valid can metadata message",
			mimeType: "interface=stream;type=pdu;schema=fbs;swc_id=42;ecu_id=4",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  42,
					Ecu_id:  4,
					Type:    Pdu.TransportMetadataCan,
					CanMetadata: &message.NCodecPduCanMessageMetadata{
						Format:       Pdu.CanMessageFormatBaseFrameFormat,
						Type:         Pdu.CanFrameTypeDataFrame,
						Interface_id: 1,
						Network_id:   2,
					},
				},
			},
		},
		{
			name:     "invalid can metadata message",
			mimeType: "interface=stream;type=pdu;schema=fbs;swc_id=42;ecu_id=99",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataCan,
				},
			},
			wantError: "no CanMessageMetadata provided",
		},
		{
			name:     "valid ip metadata message",
			mimeType: "interface=stream;type=pdu;schema=fbs;swc_id=42;ecu_id=99",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World This is a test message for the test case"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataIp,
					IpMetadata: &message.NCodecIpCanMessageMetadata{
						Eth_dst_mac:   0x0000123456789ABC,
						Eth_src_mac:   0x0000CBA987654321,
						Eth_ethertype: 1,
						Eth_tci_pcp:   2,
						Eth_tci_dei:   3,
						Eth_tci_vid:   4,
					},
				},
			},
		},
		{
			name:     "invalid ip metadata message",
			mimeType: "interface=stream;type=pdu;schema=fbs;swc_id=42;ecu_id=99",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataIp,
				},
			},
			wantError: "no IpMessageMetadata provided",
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			s := new([]byte)
			p, _ := ncodec.NewNetworkCodec[message.PduMessage](tc.mimeType, s, "test", nil)
			var err error
			assert.NoError(t, err)
			gotErr := p.Write(tc.msg)
			if tc.wantError != "" {
				assert.Error(t, gotErr)
				assert.Equal(t, fmt.Errorf("%s", tc.wantError), gotErr)
			} else {
				assert.NoError(t, gotErr)
			}
			err = p.Flush()
			assert.NoError(t, err)
		})
	}
}

func TestPduCodec_Read(t *testing.T) {
	tests := []struct {
		name      string
		mimeType  string
		msg       []*message.PduMessage
		wantError string
	}{
		{
			name:     "valid none metadata message",
			mimeType: "interface=stream;type=pdu;schema=fbs",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataNONE,
				},
			},
		},
		{
			name:     "valid can metadata message",
			mimeType: "interface=stream;type=pdu;schema=fbs",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataCan,
					CanMetadata: &message.NCodecPduCanMessageMetadata{
						Format:       Pdu.CanMessageFormatBaseFrameFormat,
						Type:         Pdu.CanFrameTypeDataFrame,
						Interface_id: 1,
						Network_id:   2,
					},
				},
			},
		},
		{
			name:     "multiple valid can metadata messages",
			mimeType: "interface=stream;type=pdu;schema=fbs",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataCan,
					CanMetadata: &message.NCodecPduCanMessageMetadata{
						Format:       Pdu.CanMessageFormatBaseFrameFormat,
						Type:         Pdu.CanFrameTypeDataFrame,
						Interface_id: 1,
						Network_id:   2,
					},
				},
				{
					Id:      456,
					Payload: []byte("foo bar"),
					Swc_id:  67,
					Ecu_id:  420,
					Type:    Pdu.TransportMetadataCan,
					CanMetadata: &message.NCodecPduCanMessageMetadata{
						Format:       Pdu.CanMessageFormatBaseFrameFormat,
						Type:         Pdu.CanFrameTypeDataFrame,
						Interface_id: 6,
						Network_id:   8,
					},
				},
			},
		},
		{
			name:     "valid ip metadata message",
			mimeType: "interface=stream;type=pdu;schema=fbs",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataIp,
					IpMetadata: &message.NCodecIpCanMessageMetadata{
						Eth_dst_mac:   0x0000123456789ABC,
						Eth_src_mac:   0x0000CBA987654321,
						Eth_ethertype: 1,
						Eth_tci_pcp:   2,
						Eth_tci_dei:   3,
						Eth_tci_vid:   4,
					},
				},
			},
		},
		{
			name:     "valid ipv4 message",
			mimeType: "interface=stream;type=pdu;schema=fbs;swc_id=42;ecu_id=99",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataIp,
					IpMetadata: &message.NCodecIpCanMessageMetadata{
						Eth_dst_mac:   0x0000123456789ABC,
						Eth_src_mac:   0x0000CBA987654321,
						Eth_ethertype: 1,
						Eth_tci_pcp:   2,
						Eth_tci_dei:   3,
						Eth_tci_vid:   4,
						Ip_addr_type:  Pdu.IpAddrv4,
						Ip_addr: message.NCodecIpAddr{
							Ip_v4: message.NCodecPduIpAddrV4{
								Src_ip: 1001,
								Dst_ip: 2002,
							},
						},
						Ip_src_port: 3003,
						Ip_dst_port: 4004,
					},
				},
			},
		},
		{
			name:     "valid ipv6 message",
			mimeType: "interface=stream;type=pdu;schema=fbs",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataIp,
					IpMetadata: &message.NCodecIpCanMessageMetadata{
						Eth_dst_mac:   0x0000123456789ABC,
						Eth_src_mac:   0x0000CBA987654321,
						Eth_ethertype: 1,
						Eth_tci_pcp:   2,
						Eth_tci_dei:   3,
						Eth_tci_vid:   4,
						Ip_addr_type:  Pdu.IpAddrv6,
						Ip_addr: message.NCodecIpAddr{
							Ip_v6: message.NCodecPduIpAddrV6{
								Src_ip: [8]uint16{0, 1, 2, 3, 4, 5, 6, 7},
								Dst_ip: [8]uint16{7, 6, 5, 4, 3, 2, 1, 0},
							},
						},
						Ip_src_port: 3003,
						Ip_dst_port: 4004,
					},
				},
			},
		},
		{
			name:     "valid do ip adapter message",
			mimeType: "interface=stream;type=pdu;schema=fbs;swc_id=42;ecu_id=99",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataIp,
					IpMetadata: &message.NCodecIpCanMessageMetadata{
						Eth_dst_mac:   0x0000123456789ABC,
						Eth_src_mac:   0x0000CBA987654321,
						Eth_ethertype: 1,
						Eth_tci_pcp:   2,
						Eth_tci_dei:   3,
						Eth_tci_vid:   4,
						So_ad_type:    Pdu.SocketAdapterdo_ip,
						So_ad: message.NCodecPduSocketAdapter{
							Do_ip: message.NCodecPduDoIpAdapter{
								Protocol_version: 4,
								Payload_type:     6,
							},
						},
					},
				},
			},
		},
		{
			name:     "valid some ip adapter message",
			mimeType: "interface=stream;type=pdu;schema=fbs;swc_id=42;ecu_id=99",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("Hello World"),
					Swc_id:  123,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataIp,
					IpMetadata: &message.NCodecIpCanMessageMetadata{
						Eth_dst_mac:   0x0000123456789ABC,
						Eth_src_mac:   0x0000CBA987654321,
						Eth_ethertype: 1,
						Eth_tci_pcp:   2,
						Eth_tci_dei:   3,
						Eth_tci_vid:   4,
						So_ad_type:    Pdu.SocketAdaptersome_ip,
						So_ad: message.NCodecPduSocketAdapter{
							Some_ip: message.NCodecPduSomeIpAdapter{
								Message_id:        10,
								Length:            11,
								Request_id:        12,
								Protocol_version:  13,
								Interface_version: 14,
								Message_type:      15,
								Return_code:       16,
							},
						},
					},
				},
			},
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			s := new([]byte)
			p, _ := ncodec.NewNetworkCodec[message.PduMessage](tc.mimeType, s, "test", nil)
			var err error
			assert.NoError(t, err)
			err = p.Write(tc.msg)
			assert.NoError(t, err)
			err = p.Flush()
			assert.NoError(t, err)

			msg, _ := p.Read()
			for i, pdu := range msg {
				assert.Equal(t, tc.msg[i].Id, pdu.Id)
				assert.Equal(t, tc.msg[i].Payload, pdu.Payload)
				assert.Equal(t, tc.msg[i].Swc_id, pdu.Swc_id)
				assert.Equal(t, tc.msg[i].Ecu_id, pdu.Ecu_id)
				assert.Equal(t, tc.msg[i].Type, pdu.Type)
				switch pdu.Type {
				case Pdu.TransportMetadataCan:
					{
						assert.Nil(t, pdu.IpMetadata)
						assert.Equal(t, tc.msg[i].CanMetadata.Format, pdu.CanMetadata.Format)
						assert.Equal(t, tc.msg[i].CanMetadata.Type, pdu.CanMetadata.Type)
						assert.Equal(t, tc.msg[i].CanMetadata.Interface_id, pdu.CanMetadata.Interface_id)
						assert.Equal(t, tc.msg[i].CanMetadata.Network_id, pdu.CanMetadata.Network_id)
					}
				case Pdu.TransportMetadataIp:
					{
						assert.Nil(t, pdu.CanMetadata)
						assert.Equal(t, tc.msg[i].IpMetadata.Eth_dst_mac, pdu.IpMetadata.Eth_dst_mac)
						assert.Equal(t, tc.msg[i].IpMetadata.Eth_src_mac, pdu.IpMetadata.Eth_src_mac)
						assert.Equal(t, tc.msg[i].IpMetadata.Eth_ethertype, pdu.IpMetadata.Eth_ethertype)
						assert.Equal(t, tc.msg[i].IpMetadata.Eth_tci_pcp, pdu.IpMetadata.Eth_tci_pcp)
						assert.Equal(t, tc.msg[i].IpMetadata.Eth_tci_dei, pdu.IpMetadata.Eth_tci_dei)
						assert.Equal(t, tc.msg[i].IpMetadata.Eth_tci_vid, pdu.IpMetadata.Eth_tci_vid)

						assert.Equal(t, tc.msg[i].IpMetadata.Ip_protocol, pdu.IpMetadata.Ip_protocol)
						assert.Equal(t, tc.msg[i].IpMetadata.Ip_addr_type, pdu.IpMetadata.Ip_addr_type)
						assert.Equal(t, tc.msg[i].IpMetadata.Ip_addr, pdu.IpMetadata.Ip_addr)
						assert.Equal(t, tc.msg[i].IpMetadata.Ip_src_port, pdu.IpMetadata.Ip_src_port)
						assert.Equal(t, tc.msg[i].IpMetadata.Ip_dst_port, pdu.IpMetadata.Ip_dst_port)
						assert.Equal(t, tc.msg[i].IpMetadata.So_ad_type, pdu.IpMetadata.So_ad_type)
						assert.Equal(t, tc.msg[i].IpMetadata.So_ad, pdu.IpMetadata.So_ad)
					}
				case Pdu.TransportMetadataNONE:
					{
						assert.Nil(t, pdu.CanMetadata)
						assert.Nil(t, pdu.IpMetadata)
					}
				}
			}
		})
	}
}

func TestPduCodec_Truncate(t *testing.T) {
	tests := []struct {
		name           string
		mimeType       string
		msg            []*message.PduMessage
		wantError      string
		expectedLength int
	}{
		{
			name:     "successful truncate after write",
			mimeType: "interface=stream;type=pdu;schema=fbs",
			msg: []*message.PduMessage{
				{
					Id:      42,
					Payload: []byte("Hello World"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataNONE,
				},
			},
			expectedLength: 84,
		},
		{
			name:     "truncate with multiple messages",
			mimeType: "interface=stream;type=pdu;schema=fbs",
			msg: []*message.PduMessage{
				{
					Id:      123,
					Payload: []byte("First message"),
					Swc_id:  42,
					Ecu_id:  99,
					Type:    Pdu.TransportMetadataNONE,
				},
				{
					Id:      456,
					Payload: []byte("Second message"),
					Swc_id:  43,
					Ecu_id:  100,
					Type:    Pdu.TransportMetadataNONE,
				},
			},
			expectedLength: 132,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			s := new([]byte)
			p, _ := ncodec.NewNetworkCodec[message.PduMessage](tc.mimeType, s, "test", nil)
			var err error

			assert.NoError(t, err)

			// Write some data
			err = p.Write(tc.msg)
			assert.NoError(t, err)
			err = p.Flush()
			assert.NoError(t, err)

			// Verify data was written
			assert.Equal(t, len(*s), tc.expectedLength)

			// Now truncate
			err = p.Truncate()
			assert.NoError(t, err)
			assert.Equal(t, len(*s), 0)
		})
	}
}
