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

	traceFile string
	csv       bool
}

func NewConvertCommand(name string) *ConvertCommand {
	c := &ConvertCommand{
		Command: command.Command{
			Name:    name,
			FlagSet: flag.NewFlagSet(name, flag.ExitOnError),
		},
	}
	c.FlagSet().BoolVar(&c.csv, "csv", true, "convert to CSV measurement format")
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
	var v trace.Visitor
	trace := trace.Stream{File: c.traceFile}

	// Select and configure the specified visitor.
	if c.csv {
		csv := NewCsv(c.traceFile)
		defer csv.Close()
		v = csv
	} else {
		return fmt.Errorf("trace convert visitor not specified")
	}

	// Process the trace.
	err := trace.Process(&v)
	return err
}
