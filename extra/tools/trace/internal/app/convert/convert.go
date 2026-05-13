// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package convert

import (
	"flag"
	"fmt"

	"github.com/boschglobal/dse.clib/extra/go/command"
	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"
)

type ConvertCommand struct {
	command.Command

	traceFile       string
	csv             bool
	asc             bool
	measurementName string // Only used for NCodec traces.
	txEcuId         uint
}

func NewConvertCommand(name string) *ConvertCommand {
	c := &ConvertCommand{
		Command: command.Command{
			Name:    name,
			FlagSet: flag.NewFlagSet(name, flag.ExitOnError),
		},
	}
	c.FlagSet().BoolVar(&c.csv, "csv", false, "convert to CSV measurement format (scalar channels)")
	c.FlagSet().BoolVar(&c.asc, "asc", false, "convert to ASC measurement format (network channels)")
	c.FlagSet().StringVar(&c.measurementName, "name", "", "measurement name (NCodec trace only)")
	c.FlagSet().UintVar(&c.txEcuId, "tx", 0, "Tx ECU identifier")
	return c
}

func (c ConvertCommand) Name() string {
	return c.Command.Name
}

func (c ConvertCommand) FlagSet() *flag.FlagSet {
	return c.Command.FlagSet
}

func (c *ConvertCommand) Parse(args []string) error {
	err := c.FlagSet().Parse(args)
	if err != nil {
		return err
	}
	if c.FlagSet().NArg() == 0 {
		return fmt.Errorf("trace file not specified")
	}
	c.traceFile = c.FlagSet().Arg(0)
	return nil
}

func (c *ConvertCommand) Run() error {
	if c.csv {
		if err := c.runCsv(); err != nil {
			return err
		}
	}
	if c.asc {
		if err := c.runAsc(); err != nil {
			return err
		}
	}
	return nil
}

func (c *ConvertCommand) runCsv() error {
	var v trace.Visitor
	trace := trace.Stream{File: c.traceFile}

	// Configure and run the CSV visitor.
	csv := NewCsv(c.traceFile)
	defer csv.Close()
	v = csv
	err := trace.Process(v)
	return err
}

func (c *ConvertCommand) runAsc() error {
	var v trace.Visitor
	trace := trace.Stream{File: c.traceFile}

	// Configure and run the CSV visitor.
	asc := NewAsc(c.traceFile, c.measurementName, uint32(c.txEcuId))
	defer asc.Close()
	v = asc
	err := trace.Process(v)
	return err
}
