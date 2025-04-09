// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package simer

import (
	"testing"

	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/pkg/file/handler/kind"
	"github.com/stretchr/testify/assert"
)

func TestListYamlFiles(t *testing.T) {
	paths := []string{"testdata/sim/model/input/data"}
	exts := []string{".yaml", ".yml"}
	yamlFiles := listFiles(paths, exts)
	assert.Equal(t, 2, len(yamlFiles))
	assert.Contains(t, yamlFiles, "testdata/sim/model/input/data/model.yaml")
	assert.Contains(t, yamlFiles, "testdata/sim/model/input/data/signalgroup.yaml")
}

func TestStackAnnotatedFlags(t *testing.T) {
	type testCase struct {
		flags          Flags
		yamlPath       string
		expectStepSize string
		expectEndTime  string
	}
	tests := map[string]testCase{
		"defaults, no flags set": {
			flags:          Flags{},
			yamlPath:       "",
			expectStepSize: "0.0005",
			expectEndTime:  "0.002",
		},
		"flags set": {
			flags:          Flags{StepSize: 0.002, EndTime: 0.02},
			yamlPath:       "",
			expectStepSize: "0.002",
			expectEndTime:  "0.02",
		},
		"annotations, no flags set": {
			flags:          Flags{},
			yamlPath:       "testdata/stackannotated/flags",
			expectStepSize: "0.0005",
			expectEndTime:  "0.005",
		},
		"annotations, flags set": {
			flags:          Flags{StepSize: 0.0001, EndTime: 0.001},
			yamlPath:       "testdata/stackannotated/flags",
			expectStepSize: "0.0001",
			expectEndTime:  "0.001",
		},
		"missing annotations, flags set": {
			flags:          Flags{StepSize: 0.001, EndTime: 0.01},
			yamlPath:       "testdata/stackannotated/none",
			expectStepSize: "0.001",
			expectEndTime:  "0.01",
		},
	}

	for name, tc := range tests {
		t.Run(name, func(t *testing.T) {
			t.Parallel()
			var stackDoc *kind.KindDoc
			if len(tc.yamlPath) > 0 {
				index := IndexYamlFiles(tc.yamlPath)
				t.Log(index)
				stacks, ok := index["Stack"]
				assert.True(t, ok)
				assert.Len(t, stacks, 1)
				stackDoc = &stacks[0]
				assert.NotNil(t, stackDoc)

			}
			assert.Equal(t, tc.expectStepSize, calculateStepSize(tc.flags, stackDoc))
			assert.Equal(t, tc.expectEndTime, calculateEndTime(tc.flags, stackDoc))
		})
	}
}
