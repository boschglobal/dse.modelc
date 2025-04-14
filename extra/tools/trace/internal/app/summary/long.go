// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package summary

import (
	"bytes"
	"fmt"
	"strings"

	flatbuffers "github.com/google/flatbuffers/go"
	"github.com/vmihailenco/msgpack/v5"

	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"

	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/channel"
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/notify"
)

type Long struct {
	SignalLookup map[uint32]string
}

func (l Long) decodeSignalData(reader *bytes.Reader) (result []string) {
	result = []string{}
	signalNames := []string{}

	dec := msgpack.NewDecoder(reader)
	cLen, _ := dec.DecodeArrayLen()
	if cLen == 0 {
		return
	}

	// Decode the names (first array).
	signalNameCount, _ := dec.DecodeArrayLen()
	for _ = range signalNameCount {
		uid, err := dec.DecodeUint32()
		if err != nil {
			fmt.Printf("  <unable to decode SignalName>\n")
			continue
		}
		name, ok := l.SignalLookup[uid]
		if !ok {
			name = fmt.Sprintf("%d", uid)
		}
		signalNames = append(signalNames, name)
	}
	if cLen == 1 {
		result = signalNames
	} else if cLen == 2 {
		// Decode the values (second array).
		signalValueCount, _ := dec.DecodeArrayLen()
		for i := range signalValueCount {
			var value any
			err := dec.Decode(&value)
			if err != nil {
				fmt.Printf("  <unable to decode SignalValue>\n")
				continue
			}
			switch v := value.(type) {
			case float64:
				row := fmt.Sprintf("%s=%f", signalNames[i], v)
				result = append(result, row)
			case []byte:
				row := fmt.Sprintf("%s=len(%d)", signalNames[i], len(v))
				result = append(result, row)
			default:
				fmt.Printf("  <unable to decode SignalValue>\n")
				continue
			}
		}
	}

	return
}

func (l *Long) VisitChannelMsg(cm trace.ChannelMsg) {
	messageName := channel.EnumNamesMessageType[cm.Msg.MessageType()]
	fmt.Printf("%s:%d:%s:%d:%d\n", messageName, cm.Msg.ModelUid(), cm.Msg.ChannelName(), cm.Msg.Token(), cm.Msg.Rc())

	switch msgType := cm.Msg.MessageType(); msgType {
	case channel.MessageTypeModelRegister:
	case channel.MessageTypeSignalIndex:
		ut := new(flatbuffers.Table)
		if ok := cm.Msg.Message(ut); !ok {
			fmt.Printf("  <unable to decode SignalIndex>\n")
			return
		}
		siMsg := new(channel.SignalIndex)
		siMsg.Init(ut.Bytes, ut.Pos)
		for i := range siMsg.IndexesLength() {
			slTable := new(channel.SignalLookup)
			siMsg.Indexes(slTable, i)
			fmt.Printf("%s:%d:%s:%s=%d\n", messageName, cm.Msg.ModelUid(), cm.Msg.ChannelName(), slTable.Name(), slTable.SignalUid())
			if slTable.SignalUid() != 0 {
				l.SignalLookup[slTable.SignalUid()] = string(slTable.Name())
			}
		}
	case channel.MessageTypeSignalValue:
		ut := new(flatbuffers.Table)
		if ok := cm.Msg.Message(ut); !ok {
			fmt.Printf("  <unable to decode SignalValue>\n")
			return
		}
		srMsg := new(channel.SignalValue)
		srMsg.Init(ut.Bytes, ut.Pos)
		reader := bytes.NewReader(srMsg.DataBytes())
		sd := l.decodeSignalData(reader)
		for _, row := range sd {
			fmt.Printf("%s:%d:%s:%s\n", messageName, cm.Msg.ModelUid(), cm.Msg.ChannelName(), row)
		}
	case channel.MessageTypeSignalRead:
		ut := new(flatbuffers.Table)
		if ok := cm.Msg.Message(ut); !ok {
			fmt.Printf("  <unable to decode SignalRead>\n")
			return
		}
		srMsg := new(channel.SignalRead)
		srMsg.Init(ut.Bytes, ut.Pos)
		reader := bytes.NewReader(srMsg.DataBytes())
		sd := l.decodeSignalData(reader)
		for _, row := range sd {
			fmt.Printf("%s:%d:%s:%s\n", messageName, cm.Msg.ModelUid(), cm.Msg.ChannelName(), row)
		}
	case channel.MessageTypeModelExit:
	}
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

		reader := bytes.NewReader(sv.DataBytes())
		sd := l.decodeSignalData(reader)
		for _, row := range sd {
			fmt.Printf("[%f] Notify[%d]:SignalVector:%s:Signal:%s\n", nm.Msg.ModelTime(), sv.ModelUid(), sv.Name(), row)
		}
	}

}
