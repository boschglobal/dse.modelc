// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package summary

import (
	"fmt"
	"strings"

	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/channel"
)

type Short struct {
}

func (s *Short) VisitChannelMsg(cm trace.ChannelMsg) {
	messageName := channel.EnumNamesMessageType[cm.Msg.MessageType()]
	fmt.Printf("%s:%d:%d:%d::%s\n", cm.Msg.ChannelName(), cm.Msg.ModelUid(), cm.Msg.Token(), cm.Msg.Rc(), messageName)
}

func (s *Short) VisitNotifyMsg(nm trace.NotifyMsg) {
	direction := func() string {
		if nm.Msg.ModelUidLength() > 0 {
			return "(S)<--(M)"
		} else {
			return "(S)-->(M)"
		}
	}()
	uids := []string{}
	for i := 0; i < nm.Msg.ModelUidLength(); i++ {
		uids = append(uids, fmt.Sprintf("%d", nm.Msg.ModelUid(i)))
	}
	if len(uids) == 0 {
		uids = append(uids, "0") // SimBus.
	}
	fmt.Printf("Notify:%f:%f:%f %s (%s)\n", nm.Msg.ModelTime(), nm.Msg.NotifyTime(), nm.Msg.ScheduleTime(), direction, strings.Join(uids, ","))
}
