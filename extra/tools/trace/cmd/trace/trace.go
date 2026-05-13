// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"flag"
	"log/slog"
	"os"

	"github.com/boschglobal/dse.clib/extra/go/command"
	"github.com/boschglobal/dse.modelc/extra/tools/trace/internal/app/convert"
	"github.com/boschglobal/dse.modelc/extra/tools/trace/internal/app/summary"
)

var cmds = []command.CommandRunner{
	command.NewHelpCommand("help"),
	summary.NewSummaryCommand("summary"),
	convert.NewConvertCommand("convert"),
}

var usage = `
Trace tools for working with SimBus and NCodec trace files.

Usage:

	trace [--verbose] <command> [option] <trace file>

	trace convert [--csv, --asc] [--name <net name>] [--tx <ecu id>] <trace file>
	trace summary [--short, --long] <trace file>


	trace convert --csv simbus.bin
	trace convert --asc simbus.bin
	trace convert --asc --name FR_1 --tx 5 ncodec.bin

`

func printUsage() {
	command.PrintUsage(usage[1:], cmds)
}

func main() {
	os.Exit(main_())
}

func main_() int {
	var verbose bool
	flag.BoolVar(&verbose, "verbose", false, "enable verbose output")
	flag.Usage = printUsage
	flag.Parse()
	if verbose {
		slog.SetLogLoggerLevel(slog.LevelDebug)
	}

	if flag.NArg() == 0 {
		printUsage()
		return 1
	}

	cmdName := flag.Arg(0)
	cmdArgs := flag.Args()[1:]
	for i, v := range cmdArgs {
		os.Args[2+i] = v
	}
	if err := command.DispatchCommand(cmdName, cmds); err != nil {
		slog.Error(err.Error())
		return 2
	}

	return 0
}
