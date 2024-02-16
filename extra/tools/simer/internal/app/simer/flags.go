// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package simer

type Flags struct {
	Stack     string
	StackList []string

	StepSize float64
	EndTime  float64

	Transport string
	Uri       string
	Timeout   float64
}
