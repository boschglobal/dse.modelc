// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package index

import (
	"io/fs"
	"os"
	"path/filepath"
)

func IndexFiles(path string, extension string) ([]string, error) {
	files := []string{}
	fileSystem := os.DirFS(".")
	err := fs.WalkDir(fileSystem, path, func(s string, d fs.DirEntry, e error) error {
		if !d.IsDir() && filepath.Ext(s) == extension {
			files = append(files, s)
		}
		return nil
	})

	return files, err
}
