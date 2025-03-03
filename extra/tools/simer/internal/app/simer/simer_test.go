// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package simer

import (
	"testing"

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
