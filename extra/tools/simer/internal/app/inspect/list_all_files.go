// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package inspect

import (
	"context"
	"fmt"
	"io/fs"
	"path/filepath"
)

type ListAllFiles struct {
}

func (l ListAllFiles) Name() string {
	return "List Files"
}

func (l ListAllFiles) Function(ctx context.Context, ins *Inspect) TaskResult {
	var manifest []string

	// WalkDir visits entries in lexical order.
	err := filepath.WalkDir(ins.simPath, func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if path == ins.simPath {
			return nil
		}

		rel, err := filepath.Rel(ins.simPath, path)
		if err != nil {
			return err
		}
		rel = filepath.ToSlash(rel)

		info, err := d.Info()
		if err != nil {
			return err
		}
		modTime := info.ModTime().UTC().Format("2006-01-02T15:04Z07:00")

		if d.IsDir() {
			manifest = append(manifest, fmt.Sprintf("d %*s %s %s/", sizeWidth, "-", modTime, rel))
			return nil
		}
		manifest = append(manifest, fmt.Sprintf("f %*s %s %s", sizeWidth, humanSize(info.Size()), modTime, rel))
		return nil
	})
	if err != nil {
		return TaskResult{TargetName: l.Name(), Err: err}
	}

	return TaskResult{
		TargetName: l.Name(),
		Details: map[string]any{
			"format":   "type(d|f) size(-=dir) modified(UTC) path",
			"manifest": manifest,
		},
	}
}

// sizeWidth fits the longest humanSize result so the timestamps line up.
const sizeWidth = 6

// humanSize formats n in binary units, e.g. 109B, 1.2K, 10.5M.
func humanSize(n int64) string {
	const unit = 1024
	if n < unit {
		return fmt.Sprintf("%dB", n)
	}
	div, exp := int64(unit), 0
	for m := n / unit; m >= unit; m /= unit {
		div *= unit
		exp++
	}
	return fmt.Sprintf("%.1f%c", float64(n)/float64(div), "KMGTPE"[exp])
}
