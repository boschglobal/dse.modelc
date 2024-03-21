// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"flag"
	"log/slog"
	"os"

	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/app/simer"
	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/pkg/session"
)

var (
	// Operational flags.
	tmux   = flag.Bool("tmux", false, "run simulation with TMUX user interface")
	gdb    = flag.String("gdb", "", "attach this model instance to GDB server")
	logger = flag.Int("logger", 3, "log level (select between 0..4)")

	// Binary flags (adjusted only during development).
	redisPath     = flag.String("redis", "redis-server", "path to redis-server executable")
	simbusPath    = flag.String("simbus", "simbus", "path to SimBus executable (set to \"\" to disable)")
	modelcPath    = flag.String("modelc", "modelc", "path to ModelC executable")
	modelcX32Path = flag.String("modelcX32", "modelc32", "path to ModelC x32 executable")
)

func main() {
	parseFlags()
	slog.SetDefault(NewLogger(*logger))
	printFlags()

	// Build commands.
	index := simer.IndexYamlFiles(".")
	cmds := []*session.Command{}
	if c := simer.RedisCommand(*redisPath); c != nil {
		cmds = append(cmds, c)
	}
	if c := simer.SimbusCommand(index, *simbusPath, flags); c != nil {
		cmds = append(cmds, c)
	}
	cmds = append(cmds, simer.ModelCommandList(index, *modelcPath, *modelcX32Path, flags)...)

	// Start and run the session.
	s := session.ConsoleSession{}
	s.Create()
	for _, c := range cmds {
		s.Attach(c)
	}
	s.Wait()
	os.Exit(0)
}
