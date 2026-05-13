// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package convert

import (
	"encoding/csv"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"
	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/notify"
)

type Csv struct {
	traceFile string

	channels map[string]*channel
}

func NewCsv(traceFile string) *Csv {
	return &Csv{
		traceFile: traceFile,
		channels:  make(map[string]*channel),
	}
}

func (c *Csv) Close() {
	for _, ch := range c.channels {
		ch.close()
	}
}

func (c *Csv) VisitStream(data []byte) {

}

func (c *Csv) VisitNotifyMsg(nm trace.NotifyMsg) {
	// Embedded ModelRegister message.
	if mReg := nm.Msg.ModelRegister(nil); mReg != nil {
		return // Discard
	}
	// Embedded ModelExit message.
	if mExit := nm.Msg.ModelExit(nil); mExit != nil {
		return // Discard
	}

	// Signal Index from SimBus.
	if siMsg := nm.Msg.SignalIndex(nil); siMsg != nil {
		chName := string(nm.Msg.ChannelName())
		ch, ok := c.channels[chName]
		if !ok {
			ch = newChannel(chName, c.traceFile)
			c.channels[ch.name] = ch
		}

		// Extract Signal UIDs and add to index.
		for i := range nm.Msg.SignalIndex(nil).IndexesLength() {
			sl := new(notify.SignalLookup)
			siMsg.Indexes(sl, i)
			uid := sl.SignalUid()
			if uid != 0 {
				if _, ok := ch.index[uid]; !ok {
					ch.index[uid] = len(ch.header)
					ch.header = append(ch.header, string(sl.Name()))
					slog.Debug(fmt.Sprintf(" signal indexed: %s, %d -> %d", string(sl.Name()), uid, ch.index[uid]))
				}
			}
		}

		return
	}

	// Notify with Signal Vectors (if not the above messages, then its this).
	if nm.Msg.ModelUidLength() > 0 {
		return // Discard, message from model.
	}
	for i := range nm.Msg.SignalsLength() {
		sv := new(notify.SignalVector)
		if ok := nm.Msg.Signals(sv, i); !ok {
			slog.Error(fmt.Sprintf("  <unable to decode SignalVector (i=%d)>", i))
			continue
		}

		// Locate the channel.
		chName := string(sv.Name())
		ch, ok := c.channels[chName]
		if !ok {
			slog.Error(fmt.Sprintf("  <unable to locate channel (%s)>", chName))
			continue
		}

		// Add the samples.
		samples := map[uint32]float64{}
		for j := range sv.SignalLength() {
			s := new(notify.Signal)
			if ok := sv.Signal(s, j); !ok {
				slog.Error(fmt.Sprintf("  <unable to decode signal (j=%d)>\n", j))
				continue
			}
			samples[s.Uid()] = s.Value()
			slog.Debug(fmt.Sprintf("  sample [%s][%d] <- %f\n", sv.Name(), s.Uid(), s.Value()))
		}

		if len(samples) == 0 {
			// No signals, or binary signal (which is not supported by CSV measurement).
			continue
		}

		// Write the sample
		if !ch.locked {
			ch.writeHeader()
			ch.locked = true
		}
		ch.writeSample(nm.Msg.ModelTime(), samples)
	}
}

type channel struct {
	name string

	filename string
	file     *os.File
	writer   *csv.Writer

	header []string
	sample []string
	locked bool

	index map[uint32]int // Signal UID -> signal/sample offset

}

func newChannel(name string, tracefile string) *channel {
	c := &channel{
		name:  name,
		index: make(map[uint32]int),
	}

	dir := filepath.Dir(tracefile)
	base := filepath.Base(tracefile)
	ext := filepath.Ext(base)
	stem := strings.TrimSuffix(base, ext)
	c.filename = filepath.Join(dir, stem+"."+name+".csv")

	// First column is for the time.
	c.header = append(c.header, "time")
	return c
}

func (c *channel) writeHeader() {
	if len(c.header) <= 1 {
		// No signals were identified for this channel, don't create the
		// measurement file.
		return
	}

	// Create the measurement file.
	fmt.Printf("Create measurement file: %s\n", c.filename)
	file, err := os.Create(c.filename)
	if err != nil {
		panic(err)
	}
	c.file = file
	c.writer = csv.NewWriter(c.file)

	// Write the header row.
	if err := c.writer.Write(c.header); err != nil {
		panic(err)
	}
	c.sample = make([]string, len(c.header))
	for i := range c.sample {
		c.sample[i] = "0"
	}
}

func (c *channel) writeSample(simulationTime float64, values map[uint32]float64) {
	c.sample[0] = strconv.FormatFloat(simulationTime, 'f', 6, 64)
	for uid, value := range values {
		idx, ok := c.index[uid]
		if ok && idx > 0 && idx < len(c.sample) {
			c.sample[idx] = strconv.FormatFloat(value, 'f', -1, 64)
		}
	}
	if err := c.writer.Write(c.sample); err != nil {
		panic(err)
	}
}

func (c *channel) close() {
	if c.writer != nil {
		c.writer.Flush()
	}
	if c.file != nil {
		if err := c.file.Close(); err != nil {
			panic(err)
		}
	}
}
