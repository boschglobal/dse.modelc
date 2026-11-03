// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"os"

	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/app/inspect"
)

var (
	// Flags.
	help = flag.Bool("help", false, "display usage information")
	// Operational flags.
	simDir = flag.String("dir", "/sim", "path to simulation folder")
	logger = flag.Int("logger", 3, "log level (select between 0..4)")
	// InspectTask flags
	inventory = flag.Bool("inventory", true, "print simulation inventory")
	allFiles  = flag.Bool("all-files", false, "list all files in the simulation")
)

var usage = `Usage: inspect [OPTION]... TASK...

Inspect a simulation and print the results as Markdown.
At least one TASK must be selected.

Tasks:
  --inventory        print the simulation inventory (default: true)
  --all-files        list all files and folders in the simulation

Options:
  --dir PATH         path to the simulation folder (default: /sim)
  --logger LEVEL     log level, 0 (debug) to 4 (error) (default: 3)
  --help             display this help and exit

Examples:
  inspect --all-files
  inspect --dir path/to/sim --all-files
  docker run --rm -v "$(pwd)":/sim inspect:test --all-files
`

func printUsage() {
	fmt.Fprint(flag.CommandLine.Output(), usage)
}

func newLogger(level int) *slog.Logger {
	var l slog.Level
	switch level {
	case 0, 1:
		l = slog.LevelDebug
	case 2:
		l = slog.LevelInfo
	case 3:
		l = slog.LevelWarn
	case 4:
		l = slog.LevelError
	default:
		l = slog.LevelWarn
	}
	return slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: l}))
}

func main() {
	flag.Usage = printUsage
	flag.Parse()
	if *help {
		printUsage()
		os.Exit(0)
	}
	slog.SetDefault(newLogger(*logger))

	cwd, err := os.Getwd()
	if err != nil {
		slog.Error("Failed to get current working directory", "error", err)
		os.Exit(1)
	}
	slog.Info(fmt.Sprintf("Working directory: %s", cwd))
	slog.Info(fmt.Sprintf("Simulation directory: %s", *simDir))

	// Select tasks.
	var opts []inspect.InspectOption
	if *inventory {
		opts = append(opts, inspect.WithTask(inspect.Inventory{}))
	}
	if *allFiles {
		opts = append(opts, inspect.WithTask(inspect.ListAllFiles{}))
	}
	if len(opts) == 0 {
		slog.Error("No inspect task selected (e.g. -all-files)")
		printUsage()
		os.Exit(2)
	}

	// Run Inspect.
	ins := inspect.NewInspect(*simDir, opts...)
	if err := ins.Run(context.Background()); err != nil {
		slog.Error("Inspect failed", "error", err)
		os.Exit(1)
	}
}
