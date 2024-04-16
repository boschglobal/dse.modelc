// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package session

import (
	"fmt"
	"log/slog"
	"os"
	"os/exec"
	"sync"
)

type Session interface {
	Create() error
	Attach(command *Command) error
	Wait() error
}

type Command struct {
	cmd    *exec.Cmd
	wg     sync.WaitGroup
	prefix string

	Name string

	Prog string
	Args []string
	Dir  string
	Env  map[string]string

	Quiet      bool
	KillNoWait bool
}

func (c Command) Environ() []string {
	e := make([]string, 0)
	for k, v := range c.Env {
		e = append(e, fmt.Sprintf("%s=%s", k, v))
	}
	return e
}

func (c *Command) Execute() error {

	fmt.Println(c.Prog)
	cmd := exec.Command(c.Prog, c.Args...)
	if c.Dir != "" {
		cmd.Dir = c.Dir
	}
	cmd.Env = append(cmd.Environ(), c.Environ()...)

	slog.Info(fmt.Sprintf("%s", cmd.String()))
	for _, ar := range c.Args {
		fmt.Println(ar)
	}
	for _, en := range c.Environ() {
		fmt.Println(en)
	}

	out, err := cmd.CombinedOutput()
	if err != nil {
		slog.Debug(err.Error())
	}
	fmt.Printf("%s\n", out)

	return nil
}

func (c *Command) Start(redirect func(c *Command)) error {

	c.cmd = exec.Command(c.Prog, c.Args...)
	if c.Dir != "" {
		c.cmd.Dir = c.Dir
	}
	c.cmd.Env = append(c.cmd.Environ(), c.Environ()...)

	if redirect != nil {
		redirect(c)
	}

	slog.Debug("Starting command...")
	slog.Debug(fmt.Sprintf("%s", c.cmd.String()))
	for _, a := range c.cmd.Args {
		slog.Debug(fmt.Sprintf("  arg:%s:", a))
	}
	for _, en := range c.Environ() {
		slog.Debug(fmt.Sprintf("  env:%s", en))
	}

	if err := c.cmd.Start(); err != nil {
		slog.Error(err.Error())
	}

	return nil
}

func (c *Command) Run() error {
	c.cmd = exec.Command(c.Prog, c.Args...)
	if c.Dir != "" {
		c.cmd.Dir = c.Dir
	}
	c.cmd.Env = append(c.cmd.Environ(), c.Environ()...)
	c.cmd.Stdin = os.Stdin
	c.cmd.Stdout = os.Stdout
	c.cmd.Stderr = os.Stderr

	slog.Debug("Running command...")
	slog.Debug(fmt.Sprintf("%s", c.cmd.String()))
	for _, a := range c.cmd.Args {
		slog.Debug(fmt.Sprintf("  arg:%s:", a))
	}
	for _, en := range c.Environ() {
		slog.Debug(fmt.Sprintf("  env:%s:", en))
	}

	if err := c.cmd.Run(); err != nil {
		slog.Error(err.Error())
	}

	return nil
}

func (c *Command) Wait() error {
	c.wg.Wait()
	if err := c.cmd.Wait(); err != nil {
		slog.Error(err.Error())
	}

	return nil
}
