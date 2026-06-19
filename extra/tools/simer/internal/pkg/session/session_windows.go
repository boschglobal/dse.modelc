//go:build windows

// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package session

import (
	"syscall"
)

// Windows custom signal handling.
// Start cmd in new process group, signal the group with CTRL_BREAK_EVENT, after
// a grace period forced termination handling applies.
func (c *Command) installCancelHandler() {
	c.cmd.SysProcAttr = &syscall.SysProcAttr{
		CreationFlags: syscall.CREATE_NEW_PROCESS_GROUP,
	}

	if c.NoCancel {
		c.cmd.Cancel = func() error { return nil }
		c.cmd.WaitDelay = 0
		return
	}

	generateCtrlEvent := syscall.NewLazyDLL("kernel32.dll").NewProc("GenerateConsoleCtrlEvent")
	c.cmd.Cancel = func() error {
		if c.cmd.Process == nil || c.cmd.Process.Pid <= 0 {
			return nil
		}
		r, _, err := generateCtrlEvent.Call(uintptr(syscall.CTRL_BREAK_EVENT), uintptr(c.cmd.Process.Pid))
		if r == 0 {
			return err
		}
		return nil
	}
	c.cmd.WaitDelay = ProcessTerminateGracePeriod
}
