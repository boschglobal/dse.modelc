// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package summary

import (
	"flag"
	"fmt"

	"github.com/boschglobal/dse.clib/extra/go/command"
	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"
)

type SummaryCommand struct {
	command.Command

	traceFile string
	short     bool
	long      bool
}

func NewSummaryCommand(name string) *SummaryCommand {
	c := &SummaryCommand{
		Command: command.Command{
			Name:    name,
			FlagSet: flag.NewFlagSet(name, flag.ExitOnError),
		},
	}
	c.FlagSet().BoolVar(&c.short, "short", true, "generate a short summary")
	c.FlagSet().BoolVar(&c.long, "long", false, "generate a long summary")
	return c
}

func (c SummaryCommand) Name() string {
	return c.Command.Name
}

func (c SummaryCommand) FlagSet() *flag.FlagSet {
	return c.Command.FlagSet
}

func (c *SummaryCommand) Parse(args []string) error {
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

func (c *SummaryCommand) Run() error {
	var v trace.Visitor
	trace := trace.Stream{File: c.traceFile}

	// Select and configure the specified visitor.
	if c.long {
		v = &Long{SignalLookup: map[uint32]string{}}
	} else if c.short {
		v = &Short{}
	} else {
		return fmt.Errorf("trace visitor not specified")
	}

	// Process the trace.
	err := trace.Process(&v)
	return err
}
