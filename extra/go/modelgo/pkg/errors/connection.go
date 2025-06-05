// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package errors

import "fmt"

var (
	ErrNoMessage               = fmt.Errorf("no messages available")
	ErrConnTimeoutWait         = fmt.Errorf("timeout on waitmessage")
	ErrConnRedisRespIncomplete = fmt.Errorf("response incomplete")
)

type ConnectionError struct {
	msg string
	err error
}

func NewConnectionError(e error, msg string) *ConnectionError {
	return &ConnectionError{msg: msg, err: e}
}

func (e *ConnectionError) Error() string {
	if e.err != nil {
		return fmt.Sprintf("connection: %q - %v", e.msg, e.err)
	} else {
		return fmt.Sprintf("connection: %q", e.msg)
	}
}
