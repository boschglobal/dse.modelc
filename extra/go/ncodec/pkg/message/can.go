package message

import (
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/internal/schema/AutomotiveBus/Stream/Frame"
)

type CanSender struct {
	Bus_id       uint8
	Node_id      uint8
	Interface_id uint8
}

type CanTiming struct {
	send uint64
	arb  uint64
	recv uint64
}

type CanMessage struct {
	Frame_id   uint32
	Frame_type Frame.CanFrameType
	Sender     *CanSender
	Timing     *CanTiming
	Payload    []byte
}
