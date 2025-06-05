// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package errors

import "fmt"

var (
	ErrModelEndTimeReached        = fmt.Errorf("end time reached")
	ErrNotifyNotRecv              = fmt.Errorf("notify message not received")
	ErrModelNoConnection          = fmt.Errorf("no connection")
	ErrModelNotifyWait            = func(e error) error { return NewModelError(e, "notify message wait failed") }
	ErrModelChannelWait           = func(e error) error { return NewModelError(e, "channel message not received") }
	ErrModelConnectFail           = func(e error) error { return NewModelError(e, "connect failed") }
	ErrModelDuplicateSignalVector = func(sv string) error { return NewModelError(nil, fmt.Sprintf("channel exists: %q", sv)) }
	ErrModelMissingSignalVector   = func(sv string) error {
		return NewModelError(nil, fmt.Sprintf("channel not in SignalVector map: %q", sv))
	}
	ErrModelUnexpectedMessageId = func(id string) error {
		return NewModelError(nil, fmt.Sprintf("unexpected message identifier: %q", id))
	}
	ErrModelMessageDecode = func(m string) error { return NewModelError(nil, m) }
)

type ModelError struct {
	msg string
	err error
}

func NewModelError(e error, msg string) *ModelError {
	return &ModelError{msg: msg, err: e}
}

func (e *ModelError) Error() string {
	if e.err != nil {
		return fmt.Sprintf("model: %q - %v", e.msg, e.err)
	} else {
		return fmt.Sprintf("model: %q", e.msg)
	}
}
