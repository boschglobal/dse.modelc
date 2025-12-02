// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"flag"
	"log/slog"
	"os"

	"github.com/boschglobal/dse.clib/extra/go/file/index"
	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/app/simer"
	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/pkg/session"
)

var (
	// Operational flags.
	tmux   = flag.Bool("tmux", false, "run simulation with TMUX user interface")
	logger = flag.Int("logger", 3, "log level (select between 0..4)")
	// Binary flags (adjusted only during development).
	redisPath      = flag.String("redis", "redis-server", "path to redis-server executable")
	simbusPath     = flag.String("simbus", "/usr/local/bin/simbus", "path to SimBus executable (set to \"\" to disable)")
	modelcPath     = flag.String("modelc", "/usr/local/bin/modelc", "path to ModelC executable")
	modelcX32Path  = flag.String("modelcX32", "/usr/local/bin/modelc32_x86", "path to ModelC x32 executable")
	modelcI386Path = flag.String("modelcI386", "/usr/local/bin/modelc32_i386", "path to ModelC i386 executable")
)

func main() {
	parseFlags()
	slog.SetDefault(NewLogger(*logger))
	printFlags()

	cmds := []*session.Command{}
	var index = index.NewYamlFileIndex()
	index.Scan(".")
	if len(index.FileMap) == 0 {
		slog.Error("No files indexed!", "pwd", func() string {
			dir, _ := os.Getwd()
			return dir
		}())
		os.Exit(1)
	}

	// Redis.
	quietRedis := true
	if *logger < 3 {
		quietRedis = false
	}
	if c := simer.RedisCommand(*redisPath, quietRedis); c != nil {
		cmds = append(cmds, c)
	}
	// Simbus and Models.
	if c := simer.SimbusCommand(index, *simbusPath, flags); c != nil {
		cmds = append(cmds, c)
	}
	cmds = append(cmds, simer.ModelCommandList(index, *modelcPath, *modelcX32Path, *modelcI386Path, flags)...)

	// Start and run the session.
	var s session.Session
	if *tmux == true {
		s = session.NewTmuxSession()
	} else {
		s = &session.ConsoleSession{}
	}
	s.Create()
	for _, c := range cmds {
		s.Attach(c)
	}
	if err := s.Wait(); err != nil {
		slog.Error("One or more commands failed!")
		os.Exit(1)
	}
	os.Exit(0)
}
