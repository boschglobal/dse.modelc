// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package command

import (
	"flag"
	"fmt"
	"os"
	"reflect"
)

type CommandRunner interface {
	Name() string
	FlagSet() *flag.FlagSet
	Parse([]string) error
	Run() error
}

type Command struct {
	Name    string
	FlagSet *flag.FlagSet
}

func PrintUsage(usage string, cmds []CommandRunner) {
	fmt.Fprint(flag.CommandLine.Output(), usage)

	for _, cmd := range cmds {
		if cmd.Name() == "help" {
			continue
		}

		fmt.Fprintf(cmd.FlagSet().Output(), "  %s\n", cmd.Name())
		fmt.Fprintf(flag.CommandLine.Output(), "    Options:\n")
		cmd.FlagSet().VisitAll(func(f *flag.Flag) {
			fmt.Fprintf(flag.CommandLine.Output(), "      -%s %s\n", f.Name, reflect.TypeOf(f.Value))
			fmt.Fprintf(flag.CommandLine.Output(), "          %s", f.Usage)
			if f.DefValue != "" {
				fmt.Fprintf(flag.CommandLine.Output(), "  (default: %s)", f.DefValue)
			}
			fmt.Fprintf(flag.CommandLine.Output(), "\n")
		})
	}
}

func DispatchCommand(name string, cmds []CommandRunner) error {
	var cmd CommandRunner
	for _, c := range cmds {
		if c.Name() == name {
			cmd = c
			break
		}
	}
	if cmd == nil {
		return fmt.Errorf("Unknown command: %s", name)
	}

	if cmd.Name() != "help" {
		fmt.Fprintf(flag.CommandLine.Output(), "Running command: %s\n", cmd.Name())
	}
	if err := cmd.Parse(os.Args[2:]); err != nil {
		return err
	}
	fmt.Fprintf(flag.CommandLine.Output(), "Options:\n")
	cmd.FlagSet().VisitAll(func(f *flag.Flag) {
		fmt.Fprintf(flag.CommandLine.Output(), "  %-15s: %s\n", f.Name, f.Value)
	})
	return cmd.Run()
}

type HelpCommand struct {
	Command
}

func NewHelpCommand(name string) *HelpCommand {
	c := &HelpCommand{
		Command: Command{
			Name:    name,
			FlagSet: flag.NewFlagSet(name, flag.ExitOnError),
		},
	}
	return c
}

func (c HelpCommand) Name() string {
	return c.Command.Name
}

func (c HelpCommand) FlagSet() *flag.FlagSet {
	return c.Command.FlagSet
}

func (c *HelpCommand) Parse(args []string) error {
	return c.FlagSet().Parse(args)
}

func (c *HelpCommand) Run() error {
	flag.Usage()
	return nil
}
