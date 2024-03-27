// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package index

import (
	"fmt"
	"io/fs"
	"log/slog"
	"os"
	"path/filepath"
)

func IndexFiles(path string, extension string) ([]string, error) {
	files := []string{}
	fileSystem := os.DirFS(".")
	err := fs.WalkDir(fileSystem, path, func(s string, d fs.DirEntry, e error) error {
		slog.Debug(fmt.Sprintf("IndexFiles: %s (%t, %s)", s, d.IsDir(), filepath.Ext(s)))
		if !d.IsDir() && filepath.Ext(s) == extension {
			files = append(files, s)
		}
		return nil
	})

	return files, err
}
