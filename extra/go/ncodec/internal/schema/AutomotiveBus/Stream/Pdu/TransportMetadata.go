// Code generated by the FlatBuffers compiler. DO NOT EDIT.

package Pdu

import "strconv"

type TransportMetadata byte

const (
	TransportMetadataNONE TransportMetadata = 0
	TransportMetadataCan  TransportMetadata = 1
	TransportMetadataIp   TransportMetadata = 2
)

var EnumNamesTransportMetadata = map[TransportMetadata]string{
	TransportMetadataNONE: "NONE",
	TransportMetadataCan:  "Can",
	TransportMetadataIp:   "Ip",
}

var EnumValuesTransportMetadata = map[string]TransportMetadata{
	"NONE": TransportMetadataNONE,
	"Can":  TransportMetadataCan,
	"Ip":   TransportMetadataIp,
}

func (v TransportMetadata) String() string {
	if s, ok := EnumNamesTransportMetadata[v]; ok {
		return s
	}
	return "TransportMetadata(" + strconv.FormatInt(int64(v), 10) + ")"
}
