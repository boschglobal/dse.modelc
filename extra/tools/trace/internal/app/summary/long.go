// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package summary

import (
	"fmt"
	"strings"

	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"

	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/notify"
)

type Long struct {
	SignalLookup map[uint32]string
}

func (l *Long) VisitNotifyMsg(nm trace.NotifyMsg) {
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
		fmt.Printf("ModelRegister:%s:%s:%d:%d\n", uids[0], nm.Msg.ChannelName(), nm.Msg.Token(), nm.Msg.Rc())
		return
	}

	// Embedded SignalIndex message.
	siMsg := nm.Msg.SignalIndex(nil)
	if siMsg != nil {
		for i := range nm.Msg.SignalIndex(nil).IndexesLength() {
			slTable := new(notify.SignalLookup)
			siMsg.Indexes(slTable, i)
			fmt.Printf("SignalIndex:%s:%s:%s=%d\n", uids[0], nm.Msg.ChannelName(), slTable.Name(), slTable.SignalUid())
			if slTable.SignalUid() != 0 {
				l.SignalLookup[slTable.SignalUid()] = string(slTable.Name())
			}
		}
		return
	}

	// Embedded ModelExit message.
	mExit := nm.Msg.ModelExit(nil)
	if mExit != nil {
		fmt.Printf("ModelExit:%s:%s:%d:%d\n", uids[0], nm.Msg.ChannelName(), nm.Msg.Token(), nm.Msg.Rc())
		return
	}

	// Notify message with Signal Vectors.
	fmt.Printf("[%f] Notify:%f:%f:%f %s (%s)\n", nm.Msg.ModelTime(), nm.Msg.ModelTime(), nm.Msg.NotifyTime(), nm.Msg.ScheduleTime(), direction, strings.Join(uids, ","))

	for i := range nm.Msg.SignalsLength() {
		sv := new(notify.SignalVector)
		if ok := nm.Msg.Signals(sv, i); !ok {
			fmt.Printf("  <unable to decode SignalVector (i=%d)>\n", i)
			continue
		}
		fmt.Printf("[%f] Notify[%d]:SignalVector:%s\n", nm.Msg.ModelTime(), sv.ModelUid(), sv.Name())

		for j := range sv.SignalLength() {
			s := new(notify.Signal)
			if ok := sv.Signal(s, j); !ok {
				fmt.Printf("  <unable to decode signal (j=%d)>\n", j)
				continue
			}
			signalName, ok := l.SignalLookup[s.Uid()]
			if !ok {
				signalName = fmt.Sprintf("%d", s.Uid())
			}
			fmt.Printf("[%f] Notify[%d]:SignalVector:%s:Signal:%s=%f\n", nm.Msg.ModelTime(), sv.ModelUid(), sv.Name(), signalName, s.Value())
		}

		for j := range sv.BinarySignalLength() {
			s := new(notify.BinarySignal)
			if ok := sv.BinarySignal(s, j); !ok {
				fmt.Printf("  <unable to decode signal (j=%d)>\n", j)
				continue
			}
			signalName, ok := l.SignalLookup[s.Uid()]
			if !ok {
				signalName = fmt.Sprintf("%d", s.Uid())
			}
			fmt.Printf("[%f] Notify[%d]:SignalVector:%s:Signal:%s=len(%d)\n", nm.Msg.ModelTime(), sv.ModelUid(), sv.Name(), signalName, s.DataLength())
		}
	}
}
