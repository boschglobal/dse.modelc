// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package signalgroup

import (
	"flag"
	"fmt"

	"github.boschdevcloud.com/fsil/fsil.go/command"
	"github.boschdevcloud.com/fsil/fsil.go/command/util"
	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type SignalGroupCommand struct {
	command.Command

	outputFile  string
	signalCount int
}

func NewSignalGroupCommand(name string) *SignalGroupCommand {
	c := &SignalGroupCommand{
		Command: command.Command{
			Name:    name,
			FlagSet: flag.NewFlagSet(name, flag.ExitOnError),
		},
	}
	c.FlagSet().StringVar(&c.outputFile, "output", "", "path to write generated signal group file")
	c.FlagSet().IntVar(&c.signalCount, "count", 100, "number of signals to generate")
	return c
}

func (c SignalGroupCommand) Name() string {
	return c.Command.Name
}

func (c SignalGroupCommand) FlagSet() *flag.FlagSet {
	return c.Command.FlagSet
}

func (c *SignalGroupCommand) Parse(args []string) error {
	return c.FlagSet().Parse(args)
}

func (c *SignalGroupCommand) Run() error {

	scalarSignals := []kind.Signal{}
	for i := 1; i <= c.signalCount; i++ {
		signal := kind.Signal{
			Signal: fmt.Sprintf("counter-%d", i),
		}
		scalarSignals = append(scalarSignals, signal)
	}
	signalVector := kind.SignalGroup{
		Kind: "SignalGroup",
		Metadata: &kind.ObjectMetadata{
			Name: util.StringPtr("data"),
			Labels: &kind.Labels{
				"channel": "data",
			},
		},
	}
	signalVector.Spec.Signals = scalarSignals

	fmt.Fprintf(flag.CommandLine.Output(), "Writing file: %s\n", c.outputFile)
	if err := util.WriteYaml(&signalVector, c.outputFile, false); err != nil {
		return err
	}
	return nil
}
