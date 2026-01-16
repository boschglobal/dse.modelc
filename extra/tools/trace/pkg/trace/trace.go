// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package trace

import (
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/notify"
)

type Visitor interface {
	VisitNotifyMsg(NotifyMsg)
}

type Flatbuffer interface {
	Accept(*Visitor)
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
