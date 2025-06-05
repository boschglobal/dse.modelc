// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"flag"
	"fmt"
	"log/slog"
	"maps"
	"os"
	"slices"

	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/connection"
	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/model"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/message"
	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/ncodec"
)

type Network struct {
	mimetype string
	ncodec   ncodec.Codec[message.PduMessage] // TODO object will hold a pointer to the binary []byte slice.
}

var StepSize = 0.0005
var StepTotal = 10
var Signals = map[string]float64{
	"counter": 0,
}
var Networks = map[string]*Network{
	"PDU": {mimetype: "interface=stream;type=pdu;schema=fbs;swc_id=1"},
}

func main() {
	if err := main_(); err != nil {
		os.Exit(101)
	}
	os.Exit(0)
}

func main_() error {
	logLevel := flag.Int("logger", 2, "log level (select between 0..4)")
	flag.Parse()
	slog.SetDefault(NewLogger(*logLevel))
	slog.Debug(fmt.Sprintf("Log level: %d", *logLevel))

	slog.Info("Go Gateway")
	gw := model.NewGateway("gateway", 42, &connection.RedisConnection{Uid: 42, Url: "redis://localhost:6379"})
	gw.ScalarSignalVector["physical"] = model.NewSignalVector[float64]("physical").Add(slices.Collect(maps.Keys(Signals)))
	gw.BinarySignalVector["network"] = model.NewSignalVector[[]byte]("network").Add(slices.Collect(maps.Keys(Networks)))

	// TODO create Network objects.
	time := gw.SimulationTime()
	for name, net := range Networks {
		var err error
		net.ncodec, err = ncodec.NewNetworkCodec[message.PduMessage](net.mimetype, gw.BinarySignalVector["network"].GetValueRef(name), gw.Name, &time)
		if err != nil {
			slog.Error("failed to create network codec", slog.Any("err", err))
		}
	}
	networkClose := func() {
		for _, net := range Networks {
			if net.ncodec != nil {
				// net.ncodec.Close()
				net.ncodec = nil
			}
		}
	}
	defer networkClose()

	if err := gw.Connect(); err != nil {
		slog.Error(fmt.Sprint(err))
		os.Exit(1)
	}
	defer gw.Disconnect()

	for step := range StepTotal {
		// Update local signals/models.
		Signals["counter"]++
		// TODO send PDU message with counter: Sprintf("counter is: %d", Signals["counter"])
		// TODO spoof the node ID on TX to avoid filtering.
		time = gw.SimulationTime()
		pduMsgs := []*message.PduMessage{
			{
				Id:      42,
				Payload: []byte(fmt.Sprintf("counter is: %.0f", Signals["counter"])),
				Swc_id:  2,
			},
		}
		for _, net := range Networks {
			if net.ncodec != nil {
				net.ncodec.Truncate()
				net.ncodec.Write(pduMsgs)
				net.ncodec.Flush()
			}
		}

		// Sync with the SimBus.
		gw.ScalarSignalVector["physical"].Set(Signals)
		if err := gw.Sync(float64(step) * StepSize); err != nil {
			fmt.Println(err)
			os.Exit(1)
		}

		// Update local signals (from SimBus).
		gw.ScalarSignalVector["physical"].Get(Signals)
		slog.Info(fmt.Sprintf("Step: Simulation time=%.6f: Signals= %v", gw.SimulationTime(), Signals))

		// TODO print the TX PDU (i.e. the echoed message).
		for _, net := range Networks {
			if net.ncodec != nil {
				TXpdu, _ := net.ncodec.Read()
				for _, pdu := range TXpdu {
					if pdu != nil {
						slog.Debug(fmt.Sprintf("TX PDU: %s", pdu.Payload))
					} else {
						slog.Error("error: TX PDU is nil")
					}
				}
			}
		}
	}

	return nil
}
