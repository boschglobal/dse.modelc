// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package util

import (
	"bytes"
	"fmt"
	"os"

	"gopkg.in/yaml.v3"
)

func WriteYaml(v interface{}, path string, append bool) error {
	var b bytes.Buffer
	enc := yaml.NewEncoder(&b)
	enc.SetIndent(2)
	if err := enc.Encode(v); err != nil {
		return fmt.Errorf("Error encoding yaml: %v", err)
	}
	enc.Close()
	if append {
		f, err := os.OpenFile(path, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
		if err != nil {
			return fmt.Errorf("Error appending yaml: %v", err)
		}
		defer f.Close()
		if _, err := f.Write([]byte("---\n")); err != nil {
			return fmt.Errorf("Error appending yaml: %v", err)
		}
		if _, err := f.Write(b.Bytes()); err != nil {
			return fmt.Errorf("Error appending yaml: %v", err)
		}
	} else {
		if err := os.WriteFile(path, b.Bytes(), 0644); err != nil {
			return fmt.Errorf("Error writing yaml: %v", err)
		}
	}
	return nil
}
