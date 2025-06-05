// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package model

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"maps"
	"slices"

	"github.com/vmihailenco/msgpack/v5"

	mgerrors "github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/errors"
)

type SignalType interface {
	float64 | []byte
}

type Signal[T SignalType] struct {
	Name  string
	Uid   uint32
	Value T
}

type SignalVector[T SignalType] struct {
	Name   string
	Signal []Signal[T]
	Index  struct {
		Name map[string]int
		Uid  map[uint32]int
	}
	delta struct {
		changed map[int]struct{} // changed by model
		updated map[int]struct{} // updated from simbus
	}
}

func NewSignalVector[T SignalType](name string) *SignalVector[T] {
	sv := SignalVector[T]{Name: name}
	sv.Index.Name = make(map[string]int)
	sv.Index.Uid = make(map[uint32]int)
	sv.delta.changed = make(map[int]struct{})
	sv.delta.updated = make(map[int]struct{})
	return &sv
}

func (sv *SignalVector[T]) Count() int {
	return len(sv.Signal)
}

func (sv *SignalVector[T]) ClearChanged() {
	clear(sv.delta.changed)
}

func (sv *SignalVector[T]) ClearUpdated() {
	clear(sv.delta.updated)
}

func (sv *SignalVector[T]) Add(signals []string) *SignalVector[T] {
	for _, n := range signals {
		if _, ok := sv.Index.Name[n]; !ok {
			var s Signal[T]
			s.Name = n
			s.Uid = 0
			sv.Signal = append(sv.Signal, s)
			index := len(sv.Signal) - 1
			sv.Index.Name[n] = index
		}
	}
	return sv
}

func (sv *SignalVector[T]) indexSignals(name []string, uid []uint32) error {
	if len(name) != len(uid) {
		return errors.New("different count for name and uid slice")
	}
	for i, n := range name {
		if _, ok := sv.Index.Name[n]; !ok {
			sv.Add([]string{n})
		}
		// Set the UID Index (Name index is set by Add()).
		sv.Index.Uid[uid[i]] = sv.Index.Name[name[i]]

		// Update the UID field of the Signal.
		s := &sv.Signal[sv.Index.Name[name[i]]]
		s.Uid = uid[i]
		slog.Debug(fmt.Sprintf("    SignalIndex: %d [name=%s]", s.Uid, s.Name))
	}
	return nil
}

func (sv *SignalVector[T]) setValue(index int, value T) error {
	s := &sv.Signal[index]
	switch any(s.Value).(type) {
	case float64:
		if val, ok := any(value).(float64); ok {
			if any(s.Value).(float64) != val {
				s.Value = value
				slog.Debug(fmt.Sprintf("    SignalValue: %d = %f [name=%s]", s.Uid, val, s.Name))
			}
		} else {
			slog.Error("error: type mismatch, expected float64")
		}
	case []byte:
		if val, ok := any(value).([]byte); ok {
			val = append(any(s.Value).([]byte), val...)
			s.Value = any(val).(T)
		} else {
			slog.Error("error: type mismatch, expected []byte")
		}
	}
	return nil
}

func (sv *SignalVector[T]) Set(signals map[string]T) error {
	for n, v := range signals {
		if index, ok := sv.Index.Name[n]; ok {
			sv.setValue(index, v)
			sv.delta.changed[index] = struct{}{}
		}
	}
	return nil
}

func (sv *SignalVector[T]) SetByName(name []string, value []T) error {
	if len(name) != len(value) {
		return errors.New("different count for name and value slice")
	}
	for i, n := range name {
		if index, ok := sv.Index.Name[n]; ok {
			sv.setValue(index, value[i])
			sv.delta.changed[index] = struct{}{}
		}
	}
	return nil
}

func (sv *SignalVector[T]) setByUid(uid []uint32, value []T) error {
	if len(uid) != len(value) {
		return errors.New("different count for uid and value slice")
	}
	for i, u := range uid {
		if index, ok := sv.Index.Uid[u]; ok {
			sv.setValue(index, value[i])
			sv.delta.changed[index] = struct{}{}
		}
	}
	return nil
}

func (sv *SignalVector[T]) Get(signals map[string]T) error {
	for n := range maps.Keys(signals) {
		if index, ok := sv.Index.Name[n]; ok {
			signals[n] = sv.Signal[index].Value
		}
	}
	return nil
}

func (sv *SignalVector[T]) GetByName(name []string, value []T) error {
	if len(name) != len(value) {
		return errors.New("different count for name and value slice")
	}
	for i, n := range name {
		if index, ok := sv.Index.Name[n]; ok {
			value[i] = sv.Signal[index].Value
		}
	}
	return nil
}

func (sv *SignalVector[T]) GetValueRef(name string) *T {
	if index, ok := sv.Index.Name[name]; ok {
		return &sv.Signal[index].Value
	} else {
		return nil
	}
}

func (sv *SignalVector[T]) updateByUid(uid []uint32, value []T) error {
	if len(uid) != len(value) {
		return errors.New("different count for uid and value slice")
	}
	for i, u := range uid {
		if index, ok := sv.Index.Uid[u]; ok {
			sv.setValue(index, value[i])
			sv.delta.updated[index] = struct{}{}
		}
	}
	return nil
}

func (sv *SignalVector[T]) Reset() error {
	for i, _ := range sv.Signal {
		s := &sv.Signal[i]
		switch any(s.Value).(type) {
		case []byte:
			s.Value = any([]byte{}).(T)
		}
	}
	return nil
}

func (sv *SignalVector[T]) toMsgPack() (*bytes.Buffer, error) {
	buf := new(bytes.Buffer)
	enc := msgpack.NewEncoder(buf)

	// Encode the delta.
	delta := slices.Collect(maps.Keys(sv.delta.changed))
	enc.EncodeArrayLen(2)
	enc.EncodeArrayLen(len(delta))
	for _, i := range delta {
		enc.EncodeUint32(sv.Signal[i].Uid)
	}
	enc.EncodeArrayLen(len(delta))
	for _, i := range delta {
		switch any(sv.Signal[i].Value).(type) {
		case float64:
			enc.EncodeFloat64(any(sv.Signal[i].Value).(float64))
		case []byte:
			enc.EncodeBytes(any(sv.Signal[i].Value).([]byte))
		}
	}
	sv.ClearChanged()

	return buf, nil
}

func (sv *SignalVector[T]) fromMsgPack(buf *bytes.Buffer) error {
	reader := bytes.NewReader(buf.Bytes())
	dec := msgpack.NewDecoder(reader)

	cLen, err := dec.DecodeArrayLen()
	if err != nil {
		if errors.Is(err, io.EOF) {
			return nil
		}
		return err
	}
	if cLen != 2 {
		return mgerrors.NewMsgpackError(nil, fmt.Sprintf("unexpected container array len (%d)", cLen))
	}

	uidArrayLen, err := dec.DecodeArrayLen()
	if err != nil {
		return err
	}
	uids := []uint32{}
	for _ = range uidArrayLen {
		v, err := dec.DecodeUint32()
		if err != nil {
			return err
		}
		uids = append(uids, v)
	}

	valueArrayLen, err := dec.DecodeArrayLen()
	if err != nil {
		return err
	}
	if uidArrayLen != valueArrayLen {
		return mgerrors.NewMsgpackError(nil, fmt.Sprintf("decode vector length mismatch: %d != %d", uidArrayLen, valueArrayLen))
	}
	values := []T{}
	switch any(values).(type) {
	case []float64:
		for _ = range valueArrayLen {
			v, err := dec.DecodeFloat64()
			if err != nil {
				return err
			}
			values = append(values, any(v).(T))
		}
	case [][]byte:
		for _ = range valueArrayLen {
			v, err := dec.DecodeBytes()
			if err != nil {
				return err
			}
			values = append(values, any(v).(T))
		}
	}
	if len(uids) != len(values) {
		return mgerrors.NewMsgpackError(nil, fmt.Sprintf("decode mismatch: %d != %d", len(uids), len(values)))
	}
	return sv.updateByUid(uids, any(values).([]T))
}
