// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package session

import (
	"fmt"
	"strings"
)

type TmuxSession struct {
	Name string
	Cmd  string
}

func NewTmuxSession() *TmuxSession {
	return &TmuxSession{Name: "simer", Cmd: "/usr/bin/tmux"}
}

func (s *TmuxSession) Create() error {
	c := &Command{
		Prog: s.Cmd,
		Args: []string{"new-session", "-d", "-s", s.Name},
	}
	c.Run()

	return nil
}

func (s *TmuxSession) Attach(c *Command) error {
	// Run a Tmux command to create a new window and run a command. The
	// command is formatted as:
	//		tmux ... -e ... /bin/sh -c '<modelc command>; bash -i'
	// The shell runs in the Tmux window, and takes care of expanding the
	// ModelC command correctly. The `bash -i` keeps the Tmux window open
	// after the ModelC command completes.

	// Construct the command which will be executed in the Tmux window.
	cmd := strings.Join(append([]string{c.Prog}, c.Args...), " ")

	// Build the Tmux commands.
	tmuxArgs := []string{"new-window", "-t", s.Name}
	tmuxArgs = append(tmuxArgs, "-a")
	if len(c.Name) > 0 {
		tmuxArgs = append(tmuxArgs, "-n", c.Name)
	} else {
		tmuxArgs = append(tmuxArgs, "-n", "MODEL")
	}
	for _, env := range c.Environ() {
		tmuxArgs = append(tmuxArgs, "-e", env)
	}
	tmuxCmd := fmt.Sprintf("/bin/sh -c '%s; bash -i'", cmd)
	tmuxArgs = append(tmuxArgs, tmuxCmd)

	// Modify the Command object, and Start().
	c.Prog = s.Cmd
	c.Args = tmuxArgs
	c.Start(nil)

	return nil
}

func (s *TmuxSession) Wait() error {
	c := &Command{
		Prog: s.Cmd,
		Args: []string{"new-session", "-t", s.Name},
	}
	c.Run()

	return nil
}
