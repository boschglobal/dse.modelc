// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package count

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/boschglobal/dse.modelc/extra/tools/trace/pkg/trace"
)

func TestShortSummary(t *testing.T) {
	var v trace.Visitor = &CountVisitor{}
	trace := trace.Stream{File: "../../../testdata/simbus.bin"}
	err := trace.Process(&v)
	assert.Nil(t, err)
	assert.Equal(t, 24, int(v.(*CountVisitor).msgCount))
	assert.Equal(t, 10, int(v.(*CountVisitor).notifyCount))
}
