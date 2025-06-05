package message

type MessageType interface {
	CanMessage | PduMessage
}
