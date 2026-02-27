// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"flag"
	"fmt"
	"log/slog"
	"os"
	"runtime"

	"github.com/boschglobal/dse.clib/extra/go/file/index"
	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/app/simer"
	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/pkg/session"
)

var (
	// Flags.
	help = flag.Bool("help", false, "display usage information")
	// Operational flags.
	dir    = flag.String("dir", "", "path to simulation folder (default is current directory)")
	tmux   = flag.Bool("tmux", false, "run simulation with TMUX user interface")
	logger = flag.Int("logger", 3, "log level (select between 0..4)")
	// Binary flags (adjusted only during development).
	redisPath      = flag.String("redis", defaultRedisPath, "path to redis-server executable")
	simbusPath     = flag.String("simbus", defaultSimbusPath, "path to SimBus executable (set to \"\" to disable)")
	modelcPath     = flag.String("modelc", defaultModelcPath, "path to ModelC executable")
	modelcX32Path  = flag.String("modelcX32", defaultModelcX32Path, "path to ModelC x32 executable")
	modelcI386Path = flag.String("modelcI386", defaultModelcI386Path, "path to ModelC i386 executable")
)

func main() {
	parseFlags()
	if *help {
		printUsage()
		os.Exit(0)
	}
	slog.SetDefault(NewLogger(*logger))
	printFlags()

	// Change to specified simulation directory, if specified.
	if *dir != "" {
		if err := os.Chdir(*dir); err != nil {
			slog.Error("Failed to change directory", "dir", *dir, "error", err)
			os.Exit(1)
		}
	}
	cwd, err := os.Getwd()
	if err != nil {
		slog.Error("Failed to get current working directory", "error", err)
		os.Exit(1)
	}
	slog.Info(fmt.Sprintf("Working directory: %s", cwd))

	// Disable TMUX on windows.
	if *tmux && runtime.GOOS != "linux" {
		*tmux = false
		slog.Info("TMUX flag forced <false>, not supported on Windows.")
	}

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

	// Setup Redis command.
	quietRedis := true
	if *logger < 3 {
		quietRedis = false
	}
	if c := simer.RedisCommand(*redisPath, quietRedis); c != nil {
		cmds = append(cmds, c)
	}
	// Setup Simbus and Model commands.
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
