// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"flag"
	"log/slog"
	"os"

	"github.boschdevcloud.com/fsil/fsil.go/command"
	"github.com/boschglobal/dse.modelc/extra/tools/trace/internal/app/summary"
)

var cmds = []command.CommandRunner{
	command.NewHelpCommand("help"),
	summary.NewSummaryCommand("summary"),
}

var usage = `
Trace tools for working with SimBus trace files.

Usage:

	trace <command> [option] <trace file>

	trace summary [--short, --long] <trace file>

`

func printUsage() {
	command.PrintUsage(usage[1:], cmds)
}

func main() {
	os.Exit(main_())
}

func main_() int {
	flag.Usage = printUsage
	if len(os.Args) == 1 {
		printUsage()
		return 1
	}
	if err := command.DispatchCommand(os.Args[1], cmds); err != nil {
		slog.Error(err.Error())
		return 2
	}

	return 0
}
