// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package trace

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
)

type TestVisitor struct {
	monitor []Flatbuffer
}

func (s *TestVisitor) VisitChannelMsg(cm ChannelMsg) {
	s.monitor = append(s.monitor, cm)
	fmt.Println("Visit Channel Message")
}

func (s *TestVisitor) VisitNotifyMsg(nm NotifyMsg) {
	s.monitor = append(s.monitor, nm)
	fmt.Println("Visit Notify Message")
}

func TestStreamIteratorVisitor(t *testing.T) {
	var v Visitor = &TestVisitor{}
	trace := Stream{stack: []Flatbuffer{NotifyMsg{}, ChannelMsg{}}}
	for m := range trace.Messages() {
		m.Accept(&v)
	}

	assert.Len(t, v.(*TestVisitor).monitor, 2)
	assert.IsType(t, NotifyMsg{}, v.(*TestVisitor).monitor[0])
	assert.IsType(t, ChannelMsg{}, v.(*TestVisitor).monitor[1])
}

func TestStreamProcess(t *testing.T) {
	var v Visitor = &TestVisitor{}
	trace := Stream{stack: []Flatbuffer{NotifyMsg{}, ChannelMsg{}}}
	err := trace.Process(&v)

	assert.Nil(t, err)
	assert.Len(t, v.(*TestVisitor).monitor, 2)
	assert.IsType(t, NotifyMsg{}, v.(*TestVisitor).monitor[0])
	assert.IsType(t, ChannelMsg{}, v.(*TestVisitor).monitor[1])
}

func TestSimBusStream(t *testing.T) {
	var v Visitor = &TestVisitor{}
	trace := Stream{File: "../../testdata/simbus.bin"}
	err := trace.Process(&v)

	assert.Nil(t, err)
	assert.Len(t, v.(*TestVisitor).monitor, 24)
	assert.IsType(t, ChannelMsg{}, v.(*TestVisitor).monitor[0])
	assert.IsType(t, ChannelMsg{}, v.(*TestVisitor).monitor[1])
	assert.IsType(t, NotifyMsg{}, v.(*TestVisitor).monitor[12])
	assert.IsType(t, NotifyMsg{}, v.(*TestVisitor).monitor[21])
	assert.IsType(t, ChannelMsg{}, v.(*TestVisitor).monitor[22])
	assert.IsType(t, ChannelMsg{}, v.(*TestVisitor).monitor[23])
}
