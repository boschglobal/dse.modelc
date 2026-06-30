// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package convert

import (
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/boschglobal/dse.schemas/code/go/ab/stream/pdu"
	flatbuffers "github.com/google/flatbuffers/go"
)

type NetworkMeasurement struct {
	name    string
	txEcuId uint32

	filename string
	file     *os.File
	locked   bool // Indicates the measurement has started and file is created.

	config map[uint16]frameTable
}

type frameTable struct {
	table map[uint16]frameConfig
}

type frameConfig struct {
	slotId          uint16
	baseCycle       uint8
	cycleRep        uint8
	frameTableIndex uint16
	channel         uint8
}

func NewNetworkMeasurement(name string, traceFile string, txEcuId uint32) *NetworkMeasurement {
	n := &NetworkMeasurement{
		name:    name,
		txEcuId: txEcuId,
		config:  map[uint16]frameTable{},
	}

	dir := filepath.Dir(traceFile)
	base := filepath.Base(traceFile)
	ext := filepath.Ext(base)
	stem := strings.TrimSuffix(base, ext)
	n.filename = filepath.Join(dir, stem+"."+name+".asc")

	return n
}

func (n *NetworkMeasurement) indexFlexRayFrameTable(metadata *pdu.FlexrayMetadata) error {
	var nodeIdent pdu.FlexrayNodeIdentifier
	metadata.NodeIdent(&nodeIdent)
	tbl := new(flatbuffers.Table)
	if !metadata.Metadata(tbl) {
		return fmt.Errorf("missing FlexRay config metadata")
	}
	config := new(pdu.FlexrayConfig)
	config.Init(tbl.Bytes, tbl.Pos)

	if _, ok := n.config[nodeIdent.EcuId()]; !ok {
		n.config[nodeIdent.EcuId()] = frameTable{
			table: make(map[uint16]frameConfig),
		}
	}
	table := n.config[nodeIdent.EcuId()].table

	for i := 0; i < config.FrameTableLength(); i++ {
		var lpduConfig pdu.FlexrayLpduConfig
		if !config.FrameTable(&lpduConfig, i) {
			continue
		}
		table[lpduConfig.FrameTableIndex()] = frameConfig{
			slotId:          lpduConfig.SlotId(),
			baseCycle:       lpduConfig.BaseCycle(),
			cycleRep:        lpduConfig.CycleRepetition(),
			frameTableIndex: lpduConfig.FrameTableIndex(),
			channel:         uint8(lpduConfig.Channel()),
		}
		slog.Debug("Frame Config", "EcuId", nodeIdent.EcuId(), "SlotId", lpduConfig.SlotId(), "BaseCycle", lpduConfig.BaseCycle(), "CycleRepetition", lpduConfig.CycleRepetition())
	}
	return nil
}

const ascHeader = `date %s
base hex  timestamps absolute
internal events logged
Begin Triggerblock %s
        0.0000 Start of measurement
`

const ascFooter = `End Triggerblock

`

func (n *NetworkMeasurement) writeFrontMatter() {
	fmt.Printf("Create measurement file: %s\n", n.filename)
	file, err := os.Create(n.filename)
	if err != nil {
		panic(err)
	}
	n.file = file
	t := time.Now().Format("Mon Jan 2 03:04:05.000 pm 2006")
	_, err = fmt.Fprintf(n.file, ascHeader, t, t)
	if err != nil {
		panic(err)
	}
}

func (n *NetworkMeasurement) writeMessageEvent(time float64, msg *pdu.Pdu) {
	if !n.locked {
		n.writeFrontMatter()
		n.locked = true
	}

	switch msg.TransportType() {
	case pdu.TransportMetadataCan:
		if err := n.writeCanEvent(time, msg); err != nil {
			slog.Error(fmt.Sprintf("write CAN event failed: %v", err))
		}
	case pdu.TransportMetadataFlexray:
		if err := n.writeFlexRayEvent(time, msg); err != nil {
			slog.Error(fmt.Sprintf("write FlexRay event failed: %v", err))
		}
	default:
		if err := n.writePduEvent(time, msg); err != nil {
			slog.Error(fmt.Sprintf("write PDU event failed: %v", err))
		}
	}
}

func (n *NetworkMeasurement) writePduEvent(time float64, msg *pdu.Pdu) error {
	line := fmt.Sprintf(
		"%14.4f PDU %4X  %d %s\n",
		time,
		msg.Id(),
		msg.PayloadLength(),
		formatPayload(msg.PayloadBytes()),
	)
	_, err := n.file.WriteString(line)
	return err
}

func (n *NetworkMeasurement) writeCanEvent(time float64, msg *pdu.Pdu) error {
	// CAN Metadata
	tbl := new(flatbuffers.Table)
	if !msg.Transport(tbl) {
		return fmt.Errorf("missing CAN transport metadata")
	}
	meta := new(pdu.CanMessageMetadata)
	meta.Init(tbl.Bytes, tbl.Pos)

	// ASC Event
	ch := meta.NetworkId()
	dir := "Rx"
	if n.txEcuId == msg.EcuId() {
		dir = "Tx"
	}
	canFd := false
	frameType := 0x00 // TODO: support BRS.
	switch meta.MessageFormat() {
	case pdu.CanMessageFormatBaseFrameFormat:
		canFd = false
	case pdu.CanMessageFormatExtendedFrameFormat:
		canFd = false
	case pdu.CanMessageFormatFdBaseFrameFormat:
		canFd = true
		frameType = 0x8
	case pdu.CanMessageFormatFdExtendedFrameFormat:
		canFd = true
		frameType = 0x8
	}
	line := ""
	if canFd {
		line = fmt.Sprintf(
			"%14.4f  CANFD %1d %-4s %4X      %1d %1d   %1x %2d %s\n",
			time,
			ch,
			dir,
			msg.Id(),
			1, // TODO: what is this?
			0, // TODO: what is this?
			frameType,
			msg.PayloadLength(),
			formatPayload(msg.PayloadBytes()),
		)
	} else {
		line = fmt.Sprintf(
			"%14.4f %2d      %3X               %-2s d %d %s\n", // d == data frame
			time,
			ch,
			msg.Id(),
			dir,
			msg.PayloadLength(),
			formatPayload(msg.PayloadBytes()),
		)
	}
	_, err := n.file.WriteString(line)
	return err
}

func (n *NetworkMeasurement) writeFlexRayEvent(time float64, msg *pdu.Pdu) error {
	// FlexRay Metadata
	tbl := new(flatbuffers.Table)
	if !msg.Transport(tbl) {
		return fmt.Errorf("missing FlexRay transport metadata")
	}
	metadata := new(pdu.FlexrayMetadata)
	metadata.Init(tbl.Bytes, tbl.Pos)

	switch metadata.MetadataType() {
	case pdu.FlexrayMetadataTypeConfig:
		if err := n.indexFlexRayFrameTable(metadata); err != nil {
			return fmt.Errorf("index FlexRay config: %w", err)
		}
		return nil
	case pdu.FlexrayMetadataTypeLpdu:
		break
	default:
		return nil
	}

	// LPDU
	lpduTbl := new(flatbuffers.Table)
	if !metadata.Metadata(lpduTbl) {
		return fmt.Errorf("missing FlexRay LPDU metadata")
	}
	lpdu := new(pdu.FlexrayLpdu)
	lpdu.Init(lpduTbl.Bytes, lpduTbl.Pos)
	debugPrintFr(msg, lpdu)

	var nodeIdent pdu.FlexrayNodeIdentifier
	metadata.NodeIdent(&nodeIdent)
	frameTable, ok := n.config[nodeIdent.EcuId()]
	if !ok {
		slog.Info(fmt.Sprintf("missing FlexRay frame table for ecuId=%d", nodeIdent.EcuId()))
		return nil
	}
	config, ok := frameTable.table[lpdu.FrameConfigIndex()]
	if !ok {
		slog.Info(fmt.Sprintf(
			"missing FlexRay frame config for ecuId=%d frameConfigIndex=%d",
			nodeIdent.EcuId(),
			lpdu.FrameConfigIndex(),
		))
		return nil
	}

	// ASC Event
	dir := ""
	switch lpdu.Status() {
	case 1:
		// Tx - all bus traffic is Tx, however from the perspective of the
		// monitoring node, only its Tx is Tx.
		dir = "Rx"
		if n.txEcuId == msg.EcuId() {
			dir = "Tx"
		}
	case 2:
		// NTx indicates request to transmit (i.e. pending).
		return nil
	case 3:
		// Rx indicates receive, however trace only includes Tx events.
		return nil
	case 4:
		// NRx indicate request to receive.
		return nil
	default:
		return nil
	}
	line := fmt.Sprintf(
		"%14.4f  Fr  %d %s %2d %4d %2d %2d %3x 0x00 0x0000 0 0x00 %s\n",
		time, config.channel, dir, lpdu.Cycle(), msg.Id(), config.baseCycle, config.cycleRep, msg.PayloadLength(),
		formatPayload(msg.PayloadBytes()))
	_, err := n.file.WriteString(line)
	return err
}

func (n *NetworkMeasurement) writeBackMatter() {
	if !n.locked {
		return
	}
	_, err := fmt.Fprint(n.file, ascFooter)
	if err != nil {
		panic(err)
	}
}

func formatPayload(data []byte) string {
	if len(data) == 0 {
		return ""
	}

	var b strings.Builder
	for i, v := range data {
		if i > 0 {
			b.WriteByte(' ')
		}
		fmt.Fprintf(&b, "%02x", v)
	}
	return b.String()
}

func debugFormatPayload(data []byte, cols int) string {
	if len(data) == 0 {
		return ""
	}
	if cols <= 0 {
		cols = 8
	}

	var b strings.Builder
	if len(data) > cols {
		b.WriteString("\n    ")
	}
	for i, v := range data {
		if i > 0 {
			if i%cols == 0 {
				b.WriteString("\n    ")
			} else {
				b.WriteString(" ")
			}
		}
		fmt.Fprintf(&b, "%02x", v)
	}
	return b.String()
}

func debugPrintFr(msg *pdu.Pdu, lpdu *pdu.FlexrayLpdu) {
	slog.Debug(fmt.Sprintf(
		"PDU: id=%d payloadLength=%d transportType=%d swcId=%d ecuId=%d",
		msg.Id(),
		msg.PayloadLength(),
		msg.TransportType(),
		msg.SwcId(),
		msg.EcuId(),
	))
	slog.Debug(fmt.Sprintf(
		"FlexRay LPDU: cycle=%d frameConfigIndex=%d nullFrame=%t syncFrame=%t startupFrame=%t payloadPreamble=%t status=%s macrotick=%d payload=%s",
		lpdu.Cycle(),
		lpdu.FrameConfigIndex(),
		lpdu.NullFrame(),
		lpdu.SyncFrame(),
		lpdu.StartupFrame(),
		lpdu.PayloadPreamble(),
		func() string {
			switch lpdu.Status() {
			case 1:
				return "Tx"
			case 2:
				return "NTx"
			case 3:
				return "Rx"
			case 4:
				return "NRx"
			default:
				return "None"
			}
		}(),
		lpdu.Macrotick(),
		debugFormatPayload(msg.PayloadBytes(), 16),
	))
}
