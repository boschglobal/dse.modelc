// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package model

import (
	"fmt"
	"log/slog"
	"slices"

	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/channel"

	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/connection"
	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/errors"
)

var RegisterRetry = uint(60)

type Gateway struct {
	Name               string
	Uid                uint32
	RegisterRetry      uint
	ScalarSignalVector map[string]*SignalVector[float64]
	BinarySignalVector map[string]*SignalVector[[]byte]

	connection connection.Connection

	modelTime    float64 // Simulation time (advanced from the gw time)
	scheduleTime float64 // Schedule time (how far model can advance time)

	gatewayTime float64 // The current gateway time (typically behind simulation time)
	endTime     float64 // Simulation end time.

	stepSize     float64
	clockEpsilon float64
}

func NewGateway(name string, uid uint32, connection connection.Connection) (gw *Gateway) {
	gw = &Gateway{
		Name:       name,
		Uid:        uid,
		connection: connection,
	}
	if gw.RegisterRetry == 0 {
		gw.RegisterRetry = RegisterRetry
	}
	gw.ScalarSignalVector = make(map[string]*SignalVector[float64])
	gw.BinarySignalVector = make(map[string]*SignalVector[[]byte])
	return
}

func (gw *Gateway) getAllChannelNames() (chNames []string) {
	for name, _ := range gw.ScalarSignalVector {
		chNames = append(chNames, name)
	}
	for name, _ := range gw.BinarySignalVector {
		chNames = append(chNames, name)
	}

	return
}

func (gw *Gateway) GatewayTime() float64 {
	return gw.gatewayTime
}

func (gw *Gateway) SimulationTime() float64 {
	return gw.modelTime
}

func (gw *Gateway) Connect() error {
	// Check the configuration.
	if len(gw.Name) == 0 {
		return errors.ErrGatewayConfig("gateway name not configured")
	}
	if gw.Uid == 0 {
		return errors.ErrGatewayConfig("gateway uid not configured")
	}
	if len(gw.ScalarSignalVector)+len(gw.BinarySignalVector) == 0 {
		return errors.ErrGatewayConfig("gateway signal vectors not configured")
	}

	if gw.connection == nil {
		return errors.ErrModelNoConnection
	}
	if err := gw.connection.Connect(gw.getAllChannelNames()); err != nil {
		return errors.ErrModelConnectFail(err)
	}
	msg := &Message{}

	// ModelRegister
	var registerErr error
	for _ = range gw.RegisterRetry {
		msg.reset()
		tokens := msg.ModelRegister(gw)
		for len(tokens) > 0 {
			// Wait for ACKs.
			_, err, token := WaitForChannelMessage(gw, channel.MessageTypeModelRegister)
			if err != nil {
				slog.Debug(fmt.Sprint(err))
				registerErr = err
				break
			}
			tokens = slices.DeleteFunc(tokens, func(n int32) bool {
				return n == token
			})
		}
		if len(tokens) == 0 {
			// ModelRegister are ACKed.
			break
		}
	}
	if registerErr != nil {
		return registerErr
	}

	// SignalIndex
	msg.reset()
	msg.SignalIndex(gw)
	chNames := gw.getAllChannelNames()
	for len(chNames) > 0 {
		chName, err, _ := WaitForChannelMessage(gw, channel.MessageTypeSignalIndex)
		if err != nil {
			return errors.ErrModelChannelWait(err)
		}
		chNames = slices.DeleteFunc(chNames, func(n string) bool {
			return n == chName
		})
	}

	// SignalRead
	msg.reset()
	msg.SignalRead(gw)
	chNames = gw.getAllChannelNames()
	for len(chNames) > 0 {
		chName, err, _ := WaitForChannelMessage(gw, channel.MessageTypeSignalValue)
		if err != nil {
			return errors.ErrModelChannelWait(err)
		}
		chNames = slices.DeleteFunc(chNames, func(n string) bool {
			return n == chName
		})
	}

	return nil
}

func (gw *Gateway) step() error {
	// Send notify.
	msg := &Message{}
	msg.Notify(gw)

	// Wait notify.
	found, err := WaitForNotifyMessage(gw)
	if err != nil {
		return errors.ErrModelNotifyWait(err)
	}
	if found == false {
		return errors.ErrNotifyNotRecv
	}

	// TODO Step local models ?? some kind of observer pattern??
	// TODO this is a variation on the gateway design, local models.

	return nil
}

func (gw *Gateway) Sync(time float64) error {
	// Adjust the model_time according to clock_epsilon.
	if gw.clockEpsilon > 0.0 {
		gw.modelTime += gw.clockEpsilon
	}

	// If the gateway has fallen behind the SimBus time then the gateway
	// needs to advance its time (however it wishes) until this condition is
	// satisfied. Its not possible to advance the model time directly to the
	// same time as the SimBus time because we cannot be sure that the gateway
	// modelling environment will support that. */
	if time < gw.modelTime {
		return errors.ErrGatewayBehind
	}

	// Advance the gateway as many times as necessary to reach the desired
	// model time. When this loop exits the gateway will be at the same time
	// as the SimBus time. After the call to modelc_sync() the value in
	// gw.modelTime will be the _next_ time to be used for
	// synchronisation with the SimBus - either within the while loop or on
	// the next call to model_gw_sync(). */
	for gw.modelTime <= time {
		if err := gw.step(); err != nil {
			return err
		}
	}
	gw.gatewayTime = time

	if gw.endTime > 0 && gw.endTime < gw.modelTime {
		return errors.ErrModelEndTimeReached
	}

	return nil
}

func (gw *Gateway) Disconnect() {
	if gw.connection == nil {
		return
	}
	msg := &Message{}
	msg.ModelExit(gw)
	gw.connection.Disconnect()
}
