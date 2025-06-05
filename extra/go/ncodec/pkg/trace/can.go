package trace

import (
	"fmt"

	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/message"
)

type CanTraceData[T message.MessageType] struct {
	ModelInstName  string
	SimulationTime *float64
	Identifier     string
	Wildcard       bool
	Filter         map[uint32]bool
	MimeMap        *map[string]*string
}

func (t *CanTraceData[T]) TraceRX(msg T) {
	t.traceCan("RX", any(msg).(message.CanMessage))
}

func (t *CanTraceData[T]) TraceTX(msg T) {
	t.traceCan("TX", any(msg).(message.CanMessage))
}

func (t *CanTraceData[T]) traceCan(direction string, frame message.CanMessage) error {

	if t.Identifier == "" {
		Bus_id := ""
		Node_id := ""
		Interface_id := ""
		if _bus_id := (*t.MimeMap)["Bus_id"]; _bus_id != nil {
			Bus_id = *_bus_id
		}
		if _node_id := (*t.MimeMap)["Node_id"]; _node_id != nil {
			Node_id = *_node_id
		}
		if _interface_id := (*t.MimeMap)["Interface_id"]; _interface_id != nil {
			Interface_id = *_interface_id
		}
		t.Identifier = fmt.Sprintf("%s:%s:%s", Bus_id, Node_id, Interface_id)
	}
	if !t.Wildcard {
		if !t.Filter[frame.Frame_id] {
			return nil
		}
	}
	var identifier string
	_len := len(frame.Payload)
	var b string
	if _len <= 16 {
		for _, byte := range frame.Payload {
			b += fmt.Sprintf("%02x ", byte)
		}
	} else {
		for i, byte := range frame.Payload {
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
		identifier = fmt.Sprintf("%d:%d:%d", frame.Sender.Bus_id, frame.Sender.Node_id, frame.Frame_id)
	} else {
		identifier = t.Identifier
	}
	fmt.Printf("(%s) %.6f [%s] %s %02x %d :%s", t.ModelInstName,
		*t.SimulationTime, identifier, direction, frame.Frame_id, _len, b)

	return nil
}
