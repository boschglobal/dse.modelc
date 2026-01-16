// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package trace

import (
	"fmt"
	"io"
	"iter"
	"os"

	flatbuffers "github.com/google/flatbuffers/go"

	"github.com/boschglobal/dse.schemas/code/go/dse/schemas/fbs/notify"
)

type Stream struct {
	File  string
	stack []Flatbuffer // Supports unit tests.
}

func (s Stream) Messages() iter.Seq[Flatbuffer] {
	return func(yield func(Flatbuffer) bool) {
		// Supports unit tests
		for _, m := range s.stack {
			if !yield(m) {
				return
			}
		}

		// Read from the stream.
		if len(s.File) == 0 {
			// No trace file, no more messages to yield.
			return
		}
		check := func(e error) {
			if e != nil && e != io.EOF {
				panic(e)
			}
		}
		f, err := os.Open(s.File)
		check(err)
		defer f.Close()
		for {
			// Get the size of the next flatbuffer.
			b := make([]byte, flatbuffers.SizeUint32)
			readLen, err := f.Read(b)
			check(err)
			if readLen != flatbuffers.SizeUint32 {
				break // Buffer did not contain a size.
			}
			length := flatbuffers.GetSizePrefix(b, 0)

			// Load the rest of the flatbuffer.
			flatbuffer := make([]byte, length)
			readLen, err = f.Read(flatbuffer)
			check(err)
			if uint32(readLen) != length {
				fmt.Printf("Incomplete flatbuffer, read len %d (expected %d)", readLen, length)
				break
			}

			// Create and yield the message.
			switch id := flatbuffers.GetBufferIdentifier(flatbuffer); id {
			case "SBNO":
				m := NotifyMsg{}
				m.Msg = notify.GetRootAsNotifyMessage(flatbuffer, 0)
				if !yield(m) {
					return
				}
			default:
				fmt.Printf("unsupported flatbuffer, file_identifier: %s", id)
			}
		}
	}
}

func (s Stream) Process(v *Visitor) error {
	for m := range s.Messages() {
		m.Accept(v)
	}
	return nil
}
