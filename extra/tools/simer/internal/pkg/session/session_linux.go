//go:build linux

// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package session

import (
	"syscall"
)

// Linux custom signal handling.
// Start cmd in new process group, signal the group (including spawned processes)
// with SIGTERM, after a grace period forced termination handling applies.
func (c *Command) installCancelHandler() {
	c.cmd.SysProcAttr = &syscall.SysProcAttr{
		Setpgid: true,
	}

	if c.NoCancel {
		c.cmd.Cancel = func() error { return nil }
		c.cmd.WaitDelay = 0
		return
	}

	c.cmd.Cancel = func() error {
		if c.cmd.Process == nil || c.cmd.Process.Pid <= 0 {
			return nil
		}
		if pgid, err := syscall.Getpgid(c.cmd.Process.Pid); err == nil {
			return syscall.Kill(-pgid, syscall.SIGTERM)
		} else {
			return c.cmd.Process.Signal(syscall.SIGTERM)
		}
	}
	c.cmd.WaitDelay = ProcessTerminateGracePeriod
}

func (c *Command) forceKillCommand() {
	if c.cmd == nil || c.cmd.Process == nil || c.cmd.Process.Pid <= 0 {
		return
	}

	pgid, err := syscall.Getpgid(c.cmd.Process.Pid)
	if err != nil {
		pgid = c.cmd.Process.Pid
	}
	_ = syscall.Kill(-pgid, syscall.SIGKILL)
}
