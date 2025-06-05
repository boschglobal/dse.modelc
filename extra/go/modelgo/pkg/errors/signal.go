// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package errors

import "fmt"

type MsgpackError struct {
	msg string
	err error
}

func NewMsgpackError(e error, msg string) *MsgpackError {
	return &MsgpackError{msg: msg, err: e}
}

func (e *MsgpackError) Error() string {
	if e.err != nil {
		return fmt.Sprintf("msgpack: %q - %v", e.msg, e.err)
	} else {
		return fmt.Sprintf("msgpack: %q", e.msg)
	}
}
