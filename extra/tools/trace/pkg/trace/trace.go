// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package trace

import (
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/channel"
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/notify"
)

type Visitor interface {
	VisitChannelMsg(ChannelMsg)
	VisitNotifyMsg(NotifyMsg)
}

type Flatbuffer interface {
	Accept(*Visitor)
}

type ChannelMsg struct {
	Msg *channel.ChannelMessage
}

func (cm ChannelMsg) Accept(v *Visitor) {
	if v != nil {
		(*v).VisitChannelMsg(cm)
	}
}

type NotifyMsg struct {
	Msg *notify.NotifyMessage
}

func (nm NotifyMsg) Accept(v *Visitor) {
	if v != nil {
		(*v).VisitNotifyMsg(nm)
	}
}

type Trace interface {
	Process(v Visitor) error
}
