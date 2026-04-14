// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package session

import (
	"bytes"
	"context"
	"fmt"
	"io"
	"log/slog"
	"os"
	"sync"
)

type ConsoleSession struct {
	commands []*Command
	ctx      context.Context
	cancel   context.CancelFunc
}

func (s *ConsoleSession) Create() error {
	s.ctx, s.cancel = context.WithCancel(context.Background())
	return nil
}

func (s *ConsoleSession) redirectConsole(c *Command) {
	stdoutCmd, _ := c.cmd.StdoutPipe()
	stderrCmd, _ := c.cmd.StderrPipe()
	stdout := NewPassThroughWriter(os.Stdout, []byte(c.prefix))
	stderr := NewPassThroughWriter(os.Stderr, []byte(c.prefix))

	if c.KillNoWait == false {
		c.wg.Add(2)
	}
	go func() {
		_, err := io.Copy(stdout, stdoutCmd)
		if err != nil {
			slog.Debug(err.Error())
		}
		if c.KillNoWait == false {
			c.wg.Done()
		}
	}()
	go func() {
		_, err := io.Copy(stderr, stderrCmd)
		if err != nil {
			slog.Debug(err.Error())
		}
		if c.KillNoWait == false {
			c.wg.Done()
		}
	}()
}

func (s *ConsoleSession) Attach(c *Command) error {
	if len(c.Name) > 0 {
		c.prefix = fmt.Sprintf("[%s] ", c.Name)
	} else {
		c.prefix = fmt.Sprintf("[%d] ", len(s.commands))
	}
	s.commands = append(s.commands, c)
	if c.Quiet {
		c.Start(s.ctx, nil)
	} else {
		c.Start(s.ctx, s.redirectConsole)
	}
	return nil
}

func (s *ConsoleSession) Wait() error {
	type result struct {
		err error
		cmd *Command
	}

	running := 0
	results := make(chan result, len(s.commands))

	for _, c := range s.commands {
		if c.KillNoWait == true {
			continue
		}
		running++
		go func(cmd *Command) {
			results <- result{err: cmd.Wait(), cmd: cmd}
		}(c)
	}

	var firstErr error
	cancelled := false

	for i := 0; i < running; i++ {
		r := <-results
		slog.Debug(fmt.Sprintf("Command %s terminated, result: %v", r.cmd.Name, r.err))

		if r.err == nil {
			// Normal process termination.
		} else {
			// A process failed, end the simulation.
			if !cancelled {
				cancelled = true
				if s.cancel != nil {
					s.cancel()
				}
			}
			if !isExpectedCancelErr(r.err) && firstErr == nil {
				slog.Error(fmt.Sprintf("Command %s failed with reason %v, ending the simulation", r.cmd.Name, r.err))
				firstErr = r.err
			}
		}

		if r.cmd.Name == "SimBus" {
			// Search for and kill any Redis commands.
			for _, c := range s.commands {
				if c.Name == "Redis" {
					slog.Debug("Cancelling the Redis cmd")
					c.cmd.Cancel()
				}
			}
		}
	}

	return firstErr
}

func isExpectedCancelErr(err error) bool {
	if err == nil {
		return false
	}
	if err == context.Canceled || err == context.DeadlineExceeded {
		return true
	}
	msg := err.Error()
	if msg == "signal: killed" || msg == "signal: terminated" {
		return true
	}
	return false
}

type PassThroughWriter struct {
	w      io.Writer
	mu     sync.Mutex
	prefix []byte
	buf    []byte
}

func NewPassThroughWriter(w io.Writer, prefix []byte) *PassThroughWriter {
	return &PassThroughWriter{
		w:      w,
		prefix: prefix,
	}
}

func (w *PassThroughWriter) Write(d []byte) (int, error) {
	w.mu.Lock()
	defer w.mu.Unlock()

	w.buf = append(w.buf, d...)
	for {
		i := bytes.IndexByte(w.buf, '\n')
		if i < 0 {
			// Partial line (i.e. no \n).
			break
		}
		w.w.Write(w.prefix)
		w.w.Write(w.buf[:i])
		w.w.Write([]byte("\n"))

		w.buf = w.buf[i+1:]
	}
	// io.copy() expects length of the passed slice.
	return len(d), nil
}
