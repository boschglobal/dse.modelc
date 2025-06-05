package automotivebus_test

import (
	"fmt"
	"testing"

	"github.com/boschglobal/dse.modelc/extra/go/ncodec/internal/schema/AutomotiveBus/Stream/Frame"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/message"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/ncodec"
	"github.com/stretchr/testify/assert"
)

func TestCanCodec_Write(t *testing.T) {
	tests := []struct {
		name      string
		mimeType  string
		msg       []*message.CanMessage
		wantError string
	}{
		{
			name:     "valid can metadata message",
			mimeType: "interface=stream;type=can;schema=fbs",
			msg: []*message.CanMessage{
				{
					Frame_id:   1,
					Frame_type: Frame.CanFrameTypeBaseFrame,
					Payload:    []byte("Hello World"),
					Sender: &message.CanSender{
						Bus_id:       1,
						Node_id:      2,
						Interface_id: 3,
					},
				},
			},
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			s := new([]byte)
			c, _ := ncodec.NewNetworkCodec[message.CanMessage](tc.mimeType, s, "test", nil)
			var err error
			assert.NoError(t, err)
			gotErr := c.Write(tc.msg)
			if tc.wantError != "" {
				assert.Error(t, gotErr)
				assert.Equal(t, fmt.Errorf("%s", tc.wantError), gotErr)
			} else {
				assert.NoError(t, gotErr)
			}
			err = c.Flush()
			assert.NoError(t, err)
		})
	}
}

func TestCanodec_Read(t *testing.T) {
	tests := []struct {
		name      string
		mimeType  string
		msg       []*message.CanMessage
		wantError string
	}{
		{
			name:     "valid can metadata message",
			mimeType: "interface=stream;type=can;schema=fbs;bus_id=1;node_id=2;interface_id=3",
			msg: []*message.CanMessage{
				{
					Frame_id:   1,
					Frame_type: Frame.CanFrameTypeBaseFrame,
					Payload:    []byte("Hello World"),
					Sender: &message.CanSender{
						Bus_id:       1,
						Node_id:      2,
						Interface_id: 3,
					},
				},
			},
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			s := new([]byte)
			p, _ := ncodec.NewNetworkCodec[message.CanMessage](tc.mimeType, s, "test", nil)
			var err error
			assert.NoError(t, err)
			err = p.Write(tc.msg)
			assert.NoError(t, err)
			err = p.Flush()
			assert.NoError(t, err)

			msg, _ := p.Read()
			for i, frame := range msg {
				assert.Equal(t, tc.msg[i].Frame_id, frame.Frame_id)
				assert.Equal(t, tc.msg[i].Frame_type, frame.Frame_type)
				assert.Equal(t, tc.msg[i].Payload, frame.Payload)
				assert.Equal(t, tc.msg[i].Sender.Bus_id, frame.Sender.Bus_id)
				assert.Equal(t, tc.msg[i].Sender.Node_id, frame.Sender.Node_id)
				assert.Equal(t, tc.msg[i].Sender.Interface_id, frame.Sender.Interface_id)
			}
		})
	}
}
