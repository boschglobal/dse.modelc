// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package connection

import (
	"fmt"

	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/errors"
)

type Connection interface {
	Connect(channels []string) (err error) // FIXME need channels ?
	Disconnect()
	SendMessage(msg []byte, channel string) (err error)
	WaitMessage(immediate bool) (msg []byte, channel string, err error)
	PeekMessage() (msg []byte, channel string, err error)
	Token() int32
}

type MsgBufferItem struct {
	Channel string
	Msg     []byte
}

type StubConnection struct {
	// message trace with decoded messages as list
	Stack       []MsgBufferItem
	Trace       []MsgBufferItem
	SendToStack bool
	token       int32
}

func (s *StubConnection) Token() int32 {
	s.token += 1
	return s.token
}

func (s *StubConnection) Connect(channels []string) (err error) {
	return nil
}

func (s *StubConnection) Disconnect() {

}

func (s *StubConnection) SendMessage(msg []byte, channel string) (err error) {
	// Make a deep copy of the msg as following assignments are a shallow
	// copy, which can (does) result in corrupt slices.
	_msg := make([]byte, len(msg))
	copy(_msg, msg)

	if s.SendToStack == true {
		s.Stack = append(s.Stack, MsgBufferItem{Channel: channel, Msg: _msg})
	}
	s.Trace = append(s.Trace, MsgBufferItem{Channel: channel, Msg: _msg})
	return nil
}

func (s *StubConnection) WaitMessage(immediate bool) (msg []byte, channel string, err error) {
	if len(s.Stack) > 0 {
		m := s.Stack[0]
		s.Stack = s.Stack[1:]
		s.Trace = append(s.Trace, m)
		return m.Msg, m.Channel, nil
	} else {
		return nil, "", errors.ErrNoMessage
	}
}

func (s *StubConnection) PeekMessage() (msg []byte, channel string, err error) {
	if len(s.Stack) > 0 {
		m := s.Stack[0]
		return m.Msg, m.Channel, nil
	} else {
		return nil, "", errors.ErrNoMessage
	}
}

func (s *StubConnection) PushMessage(msg []byte, channel string) (err error) {
	// Make a deep copy of the msg as following assignments are a shallow
	// copy, which can (does) result in corrupt slices.
	_msg := make([]byte, len(msg))
	copy(_msg, msg)
	s.Stack = append(s.Stack, MsgBufferItem{Channel: channel, Msg: _msg})
	return nil
}

func (s *StubConnection) TraceMessage(index int) (msg []byte, channel string, err error) {
	if len(s.Trace) > index {
		m := s.Trace[index]
		return m.Msg, m.Channel, nil
	} else {
		return nil, "", fmt.Errorf("no message at index (%d) available on connection Stack", index)
	}
}

func (s *StubConnection) Reset() {
	s.Stack = []MsgBufferItem{}
	s.Trace = []MsgBufferItem{}
}
