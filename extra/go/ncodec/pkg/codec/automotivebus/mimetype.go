package automotivebus

import (
	"fmt"
	"strings"
)

func DecodeMimeType(MimeType string) (map[string]*string, error) {
	if MimeType == "" {
		return nil, fmt.Errorf("MimeType is empty")
	}

	MimeMap := make(map[string]*string)

	parts := strings.FieldsFunc(MimeType, func(r rune) bool {
		return r == ';' || r == ' '
	})

	for _, part := range parts {
		if kv := strings.SplitN(part, "=", 2); len(kv) == 2 {
			v := strings.TrimSpace(kv[1])
			MimeMap[strings.TrimSpace(kv[0])] = &v
		}
	}
	// required parameters.
	var Guard = []string{"interface", "type", "schema"}
	for _, key := range Guard {
		if param, ok := MimeMap[key]; ok {
			switch key {
			case "type":
				if *param != "can" && *param != "pdu" {
					return nil, fmt.Errorf("unsupported type: %s", *param)
				}
			case "interface":
				if *param != "stream" {
					return nil, fmt.Errorf("wrong interface: %s", *param)
				}
			case "schema":
				if *param != "fbs" {
					return nil, fmt.Errorf("wrong schema: %s", *param)
				}
			default:
				{
					//NOP
				}
			}
		} else {
			return nil, fmt.Errorf("missing required mimetype parameter")
		}
	}
	// all possible parameters.
	var Options = []string{"type", "schema", "interface", "bus", "schema", "bus_id", "node_id", "interface_id", "swc_id", "ecu_id"}
	for key := range MimeMap {
		found := false
		for _, opt := range Options {
			if key == opt {
				found = true
				break
			}
		}
		if !found {
			return nil, fmt.Errorf("unexpected mimetype parameter: %s", key)
		}
	}
	return MimeMap, nil
}
