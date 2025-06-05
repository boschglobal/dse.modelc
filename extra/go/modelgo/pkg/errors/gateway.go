// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package errors

import "fmt"

var (
	ErrGatewayBehind = fmt.Errorf("gateway behind simulation time")
	ErrGatewayConfig = func(m string) error { return NewGatewayError(nil, m) }
)

type GatewayError struct {
	msg string
	err error
}

func NewGatewayError(e error, msg string) *GatewayError {
	return &GatewayError{msg: msg, err: e}
}

func (e *GatewayError) Error() string {
	if e.err != nil {
		return fmt.Sprintf("gateway: %q - %v", e.msg, e.err)
	} else {
		return fmt.Sprintf("gateway: %q", e.msg)
	}
}
