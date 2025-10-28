// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package count

import (
	"flag"
	"fmt"

	"github.com/boschglobal/dse.clib/extra/go/command"
	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"
)

type CountCommand struct {
	command.Command
	traceFile string
}

func NewCountCommand(name string) *CountCommand {
	c := &CountCommand{
		Command: command.Command{
			Name:    name,
			FlagSet: flag.NewFlagSet(name, flag.ExitOnError),
		},
	}
	return c
}

func (c CountCommand) Name() string {
	return c.Command.Name
}

func (c CountCommand) FlagSet() *flag.FlagSet {
	return c.Command.FlagSet
}

func (c *CountCommand) Parse(args []string) error {
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

func (c *CountCommand) Run() error {
	var v trace.Visitor = &CountVisitor{}
	trace := trace.Stream{File: c.traceFile}
	err := trace.Process(&v)
	if err != nil {
		return err
	}

	fmt.Printf("Message Count: %d\n", v.(*CountVisitor).msgCount)
	fmt.Printf("Notify Count: %d\n", v.(*CountVisitor).notifyCount)
	return nil
}
