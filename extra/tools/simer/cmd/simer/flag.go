// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"flag"
	"fmt"
	"log/slog"
	"reflect"
	"slices"
	"strings"

	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/app/simer"
)

var usage = `
SIMER (Simulation Runner from Dynamic Simulation Environment)

  Containerised simulation run-time.

Examples:
  simer -endtime 0.1
  simer -stack minimal_stack -endtime 0.1 -logger 2
  simer -stepsize 0.0005 -endtime 0.005 -transport redis -uri redis://redis:6379
  simer -tmux -endtime 0.02
  simer -gdb simbus -tmux -endtime 0.004 -logger 2 -timeout 5

Flags:
`

var (
	flags = simer.Flags{}
)

func printUsage() {
	fmt.Fprintf(flag.CommandLine.Output(), usage)
	ignore := []string{"redis", "simbus", "modelc", "modelc32"}
	flag.VisitAll(func(f *flag.Flag) {
		if slices.Contains(ignore, f.Name) == false {
			fmt.Fprintf(flag.CommandLine.Output(), "  -%s %s\n", f.Name, reflect.TypeOf(f.Value))
			if f.DefValue != "" {
				fmt.Fprintf(flag.CommandLine.Output(), "        %s (%s)\n", f.Usage, f.DefValue)
			} else {
				fmt.Fprintf(flag.CommandLine.Output(), "        %s\n", f.Usage)
			}
		}
	})
}

func parseFlags() {
	flag.Usage = printUsage
	flag.StringVar(&flags.Stack, "stack", "", "run the named simulation stack(s)")

	flag.Float64Var(&flags.StepSize, "stepsize", 0.0005, "simulation step size")
	flag.Float64Var(&flags.EndTime, "endtime", 0.0020, "simulation end time")

	flag.StringVar(&flags.Transport, "transport", "redispubsub", "SimBus transport")
	flag.StringVar(&flags.Uri, "uri", "redis://localhost:6379", "SimBus connection URI")
	flag.Float64Var(&flags.Timeout, "timeout", 60.0, "timeout")

	flag.Var(&flags.EnvModifier, "env", "environment modifier, in format '-env MODEL:NAME=VAL'")

	flag.StringVar(&flags.Gdb, "gdb", "", "run this model instance via GDB, use CSL in format '-gdb simbus,model'")
	flag.StringVar(&flags.GdbServer, "gdbserver", "", "attach this model instance to GDB server")
	flag.StringVar(&flags.Valgrind, "valgrind", "", "run this model instance via Valgrind, use CSL in format '-valgrind simbus,model'")

	flag.Parse()
	if flagSpecified("transport") == false {
		flags.Transport = ""
	}
	if flagSpecified("uri") == false {
		flags.Uri = ""
	}
	if flagSpecified("timeout") == false {
		flags.Timeout = 0.0
	}

	flags.StackList = []string{}
	for _, v := range strings.Split(flags.Stack, ";") {
		stackName := strings.TrimSpace(v)
		if slices.Contains(flags.StackList, stackName) == false {
			flags.StackList = append(flags.StackList, stackName)
		}
	}
}

func flagSpecified(name string) (found bool) {
	found = false
	flag.Visit(func(f *flag.Flag) {
		if f.Name == name {
			found = true
		}
	})
	return
}

func printFlags() {
	slog.Info("SIMER:")
	flag.VisitAll(func(f *flag.Flag) {
		slog.Info(fmt.Sprintf("  %-9s : %s", f.Name, f.Value))
	})
}
