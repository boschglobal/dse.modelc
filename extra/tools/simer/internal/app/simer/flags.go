// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package simer

import "strings"

type listFlag []string

func (l *listFlag) String() string {
	return strings.Join(*l, "::")
}

func (l *listFlag) Set(value string) error {
	*l = append(*l, value)
	return nil
}

type Flags struct {
	Stack     string
	StackList []string

	StepSize float64
	EndTime  float64

	Transport string
	Uri       string
	Timeout   float64

	EnvModifier listFlag

	Gdb      string
	Valgrind string
}
