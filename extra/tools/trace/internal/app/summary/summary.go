// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package summary

import (
	"flag"
	"fmt"

	"github.boschdevcloud.com/fsil/fsil.go/command"
	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"
)

type SummaryCommand struct {
	command.Command

	inputFile string
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
	if c.FlagSet().NArg() != 0 {
		return fmt.Errorf("input file not specified")
	}
	c.inputFile = c.FlagSet().Arg(0)
	return nil
}

func (c *SummaryCommand) Run() error {
	v := Short{}
	trace := trace.Stream{File: ""}
	for m := range trace.Messages() {
		m.Accept(v)
	}

	return nil
}
