// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package session

import (
	"bytes"
	"fmt"
	"io"
	"log/slog"
	"os"
)

type ConsoleSession struct {
	commands []*Command
}

func (s *ConsoleSession) Create() error {
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
	c.prefix = fmt.Sprintf("%d) ", len(s.commands))
	s.commands = append(s.commands, c)
	if c.Quiet {
		c.Start(nil)
	} else {
		c.Start(s.redirectConsole)
	}
	return nil
}

func (s *ConsoleSession) Wait() error {
	for _, c := range s.commands {
		if c.KillNoWait == true {
			continue
		}
		c.Wait()
	}
	return nil
}

type PassThroughWriter struct {
	w      io.Writer
	prefix []byte
}

func NewPassThroughWriter(w io.Writer, prefix []byte) *PassThroughWriter {
	return &PassThroughWriter{
		w:      w,
		prefix: prefix,
	}
}

func (w *PassThroughWriter) Write(d []byte) (int, error) {
	pos := 0
	i := 0
	for {
		i = bytes.IndexByte(d[pos:], '\n')
		if i < 0 {
			break
		}
		w.w.Write(w.prefix)
		w.w.Write(d[pos : pos+i])
		w.w.Write([]byte("\n"))
		pos += i + 1
	}
	if pos < len(d) {
		// Partial line (i.e. no \n).
		w.w.Write(d[pos:])
	}
	// io.copy() expects length of the passed slice.
	return len(d), nil
}
