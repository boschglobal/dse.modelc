// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package model

import (
	"maps"
	"slices"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestSignalVectorFloat64(t *testing.T) {
	names := []string{"one", "two", "three", "four"}
	uids := []uint32{111, 222, 333, 444}
	values := []float64{1.1, 2.2, 3.3, 4.4}

	sv := NewSignalVector[float64]("test-name")
	assert.Equal(t, "test-name", sv.Name, "name")
	assert.Equal(t, 0, sv.Count(), "count")
	assert.Equal(t, 0, len(sv.Index.Name), "sv.Index.Name")
	assert.Equal(t, 0, len(sv.Index.Uid), "sv.Index.Uid")

	sv.Add(names)
	assert.Equal(t, len(names), sv.Count(), "sv.count")
	assert.Equal(t, len(names), len(sv.Index.Name), "sv.Index.Name")
	assert.Equal(t, 0, len(sv.Index.Uid), "sv.Index.Uid")
	assert.ElementsMatch(t, names, slices.Collect(maps.Keys(sv.Index.Name)))

	// Duplicate check.
	sv.Add(names)
	assert.Equal(t, len(names), sv.Count(), "sv.count")
	assert.Equal(t, len(names), len(sv.Index.Name), "sv.Index.Name")
	assert.Equal(t, 0, len(sv.Index.Uid), "sv.Index.Uid")
	assert.ElementsMatch(t, names, slices.Collect(maps.Keys(sv.Index.Name)))

	// UID index.
	sv.indexSignals(names, uids)
	assert.Equal(t, len(names), sv.Count(), "sv.count")
	assert.Equal(t, len(names), len(sv.Index.Name), "sv.Index.Name")
	assert.Equal(t, len(uids), len(sv.Index.Uid), "sv.Index.Uid")
	assert.ElementsMatch(t, names, slices.Collect(maps.Keys(sv.Index.Name)))
	assert.ElementsMatch(t, uids, slices.Collect(maps.Keys(sv.Index.Uid)))
	for i := range len(names) {
		assert.Equal(t, sv.Index.Name[names[i]], sv.Index.Uid[uids[i]], "index")
		assert.Equal(t, sv.Index.Name[names[i]], i, "index")
		assert.Equal(t, uids[i], sv.Signal[i].Uid, "sv.Signal.Uid")
	}

	// Set
	sv.SetByName(names, values)
	for i := range len(names) {
		assert.Equal(t, values[i], sv.Signal[i].Value, "sv.Signal.value")
	}
	slices.Reverse(values)
	sv.setByUid(uids, values)
	for i := range len(names) {
		assert.Equal(t, values[i], sv.Signal[i].Value, "sv.Signal.Uid")
	}
}

func TestSignalVectorDelta(t *testing.T) {
	names := []string{"one", "two", "three", "four"}
	uids := []uint32{111, 222, 333, 444}

	sv := NewSignalVector[float64]("test-name")
	sv.Add(names)
	sv.indexSignals(names, uids)

	// Changed.
	assert.Zero(t, len(sv.delta.changed))
	sv.ClearChanged()
	sv.ClearUpdated()
	assert.Zero(t, len(sv.delta.changed))
	assert.Zero(t, len(sv.delta.updated))
	sv.SetByName([]string{"two"}, []float64{2})
	sv.SetByName([]string{"two"}, []float64{2}) // Set constraint check.
	assert.Equal(t, 1, len(sv.delta.changed))
	sv.setByUid([]uint32{333}, []float64{3})
	sv.setByUid([]uint32{333}, []float64{3}) // Set constraint check.
	assert.Equal(t, 2, len(sv.delta.changed))
	assert.Zero(t, len(sv.delta.updated))
	assert.ElementsMatch(t, slices.Collect(maps.Keys(sv.delta.changed)), []int{1, 2})
	sv.ClearChanged()
	assert.Zero(t, len(sv.delta.changed))
	assert.Zero(t, len(sv.delta.updated))

	// Updated.
	assert.Zero(t, len(sv.delta.updated))
	sv.ClearChanged()
	sv.ClearUpdated()
	assert.Zero(t, len(sv.delta.updated))
	assert.Zero(t, len(sv.delta.changed))
	sv.updateByUid([]uint32{333}, []float64{3})
	sv.updateByUid([]uint32{333}, []float64{3}) // Set constraint check.
	assert.Equal(t, 1, len(sv.delta.updated))
	assert.Zero(t, len(sv.delta.changed))
	assert.ElementsMatch(t, slices.Collect(maps.Keys(sv.delta.updated)), []int{2})
	sv.ClearUpdated()
	assert.Zero(t, len(sv.delta.updated))
	assert.Zero(t, len(sv.delta.changed))
}

func TestSignalVectorGetSet(t *testing.T) {
	signals := map[string]float64{
		"one": 0,
		"two": 0,
	}
	sv := NewSignalVector[float64]("test-name")
	sv.Add(slices.Collect(maps.Keys(signals)))

	sv.Get(signals)
	assert.Equal(t, map[string]float64{"one": 0, "two": 0}, signals)
	assert.Zero(t, len(sv.delta.changed))
	assert.Zero(t, len(sv.delta.updated))

	signals["one"] = 1
	signals["two"] = 2
	sv.Set(signals)
	assert.Equal(t, 2, len(sv.delta.changed))
	assert.Zero(t, len(sv.delta.updated))

	signals["one"] = 0
	signals["two"] = 0
	sv.Get(signals)
	assert.Equal(t, map[string]float64{"one": 1, "two": 2}, signals)
}

func TestSignalVectorGetValueRef(t *testing.T) {
	signals := map[string]*[]byte{
		"one": nil,
		"two": nil,
	}
	sv := NewSignalVector[[]byte]("test-name")
	sv.Add(slices.Collect(maps.Keys(signals)))
	for name, _ := range signals {
		signals[name] = sv.GetValueRef(name)
	}

	assert.NotNil(t, signals["one"])
	assert.NotNil(t, signals["two"])

	sv.SetByName([]string{"one"}, [][]byte{[]byte("ONE")})
	assert.Equal(t, []byte("ONE"), *signals["one"])
}

func TestSignalVectorBytes(t *testing.T) {
	names := []string{"one", "two", "three", "four"}
	uids := []uint32{111, 222, 333, 444}
	values := [][]byte{[]byte{1}, []byte{2, 2}, []byte{3, 3, 3}, []byte{4, 4, 4, 4}}

	sv := NewSignalVector[[]byte]("test-name")
	assert.Equal(t, "test-name", sv.Name, "name")
	assert.Equal(t, 0, sv.Count(), "count")
	assert.Equal(t, 0, len(sv.Index.Name), "sv.Index.Name")
	assert.Equal(t, 0, len(sv.Index.Uid), "sv.Index.Uid")

	sv.Add(names)
	assert.Equal(t, len(names), sv.Count(), "sv.count")
	assert.Equal(t, len(names), len(sv.Index.Name), "sv.Index.Name")
	assert.Equal(t, 0, len(sv.Index.Uid), "sv.Index.Uid")
	assert.ElementsMatch(t, names, slices.Collect(maps.Keys(sv.Index.Name)))

	// Duplicate check.
	sv.Add(names)
	assert.Equal(t, len(names), sv.Count(), "sv.count")
	assert.Equal(t, len(names), len(sv.Index.Name), "sv.Index.Name")
	assert.Equal(t, 0, len(sv.Index.Uid), "sv.Index.Uid")
	assert.ElementsMatch(t, names, slices.Collect(maps.Keys(sv.Index.Name)))

	// UID index.
	sv.indexSignals(names, uids)
	assert.Equal(t, len(names), sv.Count(), "sv.count")
	assert.Equal(t, len(names), len(sv.Index.Name), "sv.Index.Name")
	assert.Equal(t, len(uids), len(sv.Index.Uid), "sv.Index.Uid")
	assert.ElementsMatch(t, names, slices.Collect(maps.Keys(sv.Index.Name)))
	assert.ElementsMatch(t, uids, slices.Collect(maps.Keys(sv.Index.Uid)))
	for i := range len(names) {
		assert.Equal(t, sv.Index.Name[names[i]], sv.Index.Uid[uids[i]], "index")
		assert.Equal(t, sv.Index.Name[names[i]], i, "index")
		assert.Equal(t, uids[i], sv.Signal[i].Uid, "sv.Signal.Uid")
	}

	// Set
	sv.SetByName(names, values)
	for i := range len(names) {
		assert.Equal(t, values[i], sv.Signal[i].Value, "sv.Signal.value")
	}

	// Reset
	sv.Reset()
	for i := range len(names) {
		assert.Equal(t, 0, len(sv.Signal[i].Value), "sv.Signal.Uid")
	}

	// Append
	sv.SetByName(names, values)
	sv.setByUid(uids, values)
	for i := range len(names) {
		assert.Equal(t, func() []byte { return append(values[i], values[i]...) }(), sv.Signal[i].Value, "sv.Signal.Uid")
	}
}
