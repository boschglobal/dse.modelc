// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package summary

import (
	"fmt"
	"strings"

	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"
)

type Short struct {
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

	// Embedded ModelRegister message.
	mReg := nm.Msg.ModelRegister(nil)
	if mReg != nil {
		fmt.Printf("%s:%s:%d:%d::ModelRegister\n", nm.Msg.ChannelName(), uids[0], nm.Msg.Token(), nm.Msg.Rc())
		return
	}

	// Embedded SignalIndex message.
	siMsg := nm.Msg.SignalIndex(nil)
	if siMsg != nil {
		fmt.Printf("%s:%s:%d:%d::SignalIndex\n", nm.Msg.ChannelName(), uids[0], nm.Msg.Token(), nm.Msg.Rc())
		return
	}

	// Embedded ModelExit message.
	mExit := nm.Msg.ModelExit(nil)
	if mExit != nil {
		fmt.Printf("%s:%s:%d:%d::ModelExit\n", nm.Msg.ChannelName(), uids[0], nm.Msg.Token(), nm.Msg.Rc())
		return
	}

	// Notify message.
	fmt.Printf("Notify:%f:%f:%f %s (%s)\n", nm.Msg.ModelTime(), nm.Msg.NotifyTime(), nm.Msg.ScheduleTime(), direction, strings.Join(uids, ","))
}
