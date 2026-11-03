// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package inspect

import (
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"slices"
	"sort"
	"strconv"
	"strings"

	"github.com/boschglobal/dse.clib/extra/go/file/handler/kind"
	"github.com/boschglobal/dse.clib/extra/go/file/index"
	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

func renderStack(s InventoryStack) string {
	blocks := []string{"File: " + codeSpan(s.File) + "\n"}

	params := [][]string{
		{"name", s.Name},
		{"stacked", strconv.FormatBool(s.Stacked)},
		{"stepsize", formatFloat(s.StepSize)},
		{"endtime", formatFloat(s.EndTime)},
		// External: no simbus model, so SimBus is loopback or remote.
		{"external", strconv.FormatBool(s.SimBus.Loopback)},
	}
	blocks = append(blocks, mdTable([]string{"Parameter", "Value"}, params))

	if len(s.Env) > 0 {
		blocks = append(blocks, envTable(s.Env))
	}

	if s.Transport != nil {
		rows := [][]string{{"name", s.Transport.Kind}}
		if s.Transport.URI != "" {
			rows = append(rows, []string{"uri", codeSpan(s.Transport.URI)})
		}
		if s.Transport.Timeout != "" {
			rows = append(rows, []string{"timeout", s.Transport.Timeout})
		}
		blocks = append(blocks, mdTable([]string{"Transport", "Value"}, rows))
	}

	if ann := flatten("", s.Annotations); len(ann) > 0 {
		blocks = append(blocks, mdTable([]string{"Annotation", "Value"}, ann))
	}

	if !s.SimBus.Loopback {
		blocks = append(blocks, "### SimBus\n")
		blocks = append(blocks, renderInstance(s.SimBus.Instance)...)
	}
	for _, m := range s.Models {
		blocks = append(blocks, fmt.Sprintf("### Model: %s (%s)\n", m.Name, m.Model))
		blocks = append(blocks, renderInstance(m)...)
	}
	return strings.Join(blocks, "\n")
}

// renderInstance returns one table per block; empty tables are omitted.
func renderInstance(m InventoryModel) []string {
	operation := "started by simer"
	if m.Stacked {
		operation += " (stacked runtime)"
	}
	if m.Skipped != "" {
		operation = "not started by simer (" + m.Skipped + ")"
	}
	params := [][]string{
		{"name", m.Name},
		{"model", m.Model},
		{"uid", strconv.Itoa(m.UID)},
		{"operation", operation},
	}
	if m.Arch != "" {
		params = append(params, []string{"arch", m.Arch})
	}
	if m.MCL != "" {
		params = append(params, []string{"mcl", m.MCL})
	}
	blocks := []string{mdTable([]string{"Parameter", "Value"}, params)}

	if len(m.Channels) > 0 {
		blocks = append(blocks, channelTable(m.Channels))
	}

	if len(m.Env) > 0 {
		blocks = append(blocks, envTable(m.Env))
	}

	if len(m.Runtime) > 0 {
		blocks = append(blocks, mdTable([]string{"Runtime", "Value"}, m.Runtime))
	}

	if ann := flatten("", m.Annotations); len(ann) > 0 {
		blocks = append(blocks, mdTable([]string{"Instance Annotation", "Value"}, ann))
	}
	if ann := flatten("", m.ModelAnnotations); len(ann) > 0 {
		blocks = append(blocks, mdTable([]string{"Model Annotation", "Value"}, ann))
	}

	if len(m.Files) > 0 {
		var rows [][]string
		for _, f := range m.Files {
			rows = append(rows, []string{codeSpan(f)})
		}
		blocks = append(blocks, mdTable([]string{"Files"}, rows))
	}
	return blocks
}

func envTable(env map[string]string) string {
	keys := make([]string, 0, len(env))
	for k := range env {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	var rows [][]string
	for _, k := range keys {
		rows = append(rows, []string{k, env[k]})
	}
	return mdTable([]string{"Env", "Value"}, rows)
}

// channelTable includes the Alias, Selector and Expected Models columns only when used.
func channelTable(chs []InventoryChannel) string {
	var hasAlias, hasSelector, hasExpected bool
	for _, c := range chs {
		hasAlias = hasAlias || c.Alias != ""
		hasSelector = hasSelector || len(c.Selectors) > 0
		hasExpected = hasExpected || c.ExpectedModelCount > 0
	}
	header := []string{"Channel"}
	if hasAlias {
		header = append(header, "Alias")
	}
	if hasSelector {
		header = append(header, "Selector")
	}
	if hasExpected {
		header = append(header, "Expected Models")
	}
	var rows [][]string
	for _, c := range chs {
		row := []string{c.Name}
		if hasAlias {
			row = append(row, c.Alias)
		}
		if hasSelector {
			keys := make([]string, 0, len(c.Selectors))
			for k := range c.Selectors {
				keys = append(keys, k)
			}
			sort.Strings(keys)
			var sel []string
			for _, k := range keys {
				sel = append(sel, k+": "+c.Selectors[k])
			}
			row = append(row, strings.Join(sel, ", "))
		}
		if hasExpected {
			row = append(row, strconv.Itoa(c.ExpectedModelCount))
		}
		rows = append(rows, row)
	}
	return mdTable(header, rows)
}

func mdTable(header []string, rows [][]string) string {
	var sb strings.Builder
	sb.WriteString("| " + strings.Join(header, " | ") + " |\n")
	sb.WriteString("|" + strings.Repeat("---|", len(header)) + "\n")
	for _, r := range rows {
		cells := make([]string, len(r))
		for i, c := range r {
			cells[i] = strings.ReplaceAll(strings.ReplaceAll(c, "|", `\|`), "\n", " ")
		}
		sb.WriteString("| " + strings.Join(cells, " | ") + " |\n")
	}
	return sb.String()
}

func codeSpan(s string) string {
	if s == "" {
		return ""
	}
	return "`" + s + "`"
}

func formatFloat(f float64) string {
	return strconv.FormatFloat(f, 'f', -1, 64)
}

// flatten returns sorted dotted-key/value pairs of nested maps.
func flatten(prefix string, m map[string]any) [][]string {
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	var out [][]string
	for _, k := range keys {
		key := k
		if prefix != "" {
			key = prefix + "." + k
		}
		if sub, ok := m[k].(map[string]any); ok {
			out = append(out, flatten(key, sub)...)
			continue
		}
		out = append(out, []string{key, fmt.Sprintf("%v", m[k])})
	}
	return out
}

// Duplicated from internal/app/simer (flags removed).

func relPath(base, p string) string {
	if rel, err := filepath.Rel(base, p); err == nil {
		return filepath.ToSlash(rel)
	}
	return p
}

func listFiles(base string, paths []string, exts []string) []string {
	files := []string{}
	for _, path := range paths {
		fs.WalkDir(os.DirFS(base), path, func(s string, d fs.DirEntry, e error) error {
			if e != nil {
				return nil
			}
			if !d.IsDir() && slices.Contains(exts, filepath.Ext(s)) {
				files = append(files, s)
			}
			return nil
		})
	}
	return files
}

func findModelDoc(idx *index.YamlFileIndex, modelName string) (*kind.KindDoc, bool) {
	for _, doc := range idx.DocMap["Model"] {
		if doc.Metadata.Name == modelName {
			return &doc, true
		}
	}
	return nil, false
}

func removeDuplicate[T comparable](s []T) []T {
	seen := make(map[T]struct{}, len(s))
	return slices.DeleteFunc(s, func(val T) bool {
		_, dup := seen[val]
		seen[val] = struct{}{}
		return dup
	})
}

func stackEnv(stack *schema_kind.StackSpec) map[string]string {
	if stack.Runtime == nil || stack.Runtime.Env == nil {
		return nil
	}
	return *stack.Runtime.Env
}

func modelEnv(model *schema_kind.ModelInstance) map[string]string {
	if model.Runtime == nil || model.Runtime.Env == nil {
		return nil
	}
	return *model.Runtime.Env
}

func isRuntimeStacked(spec *schema_kind.StackSpec) bool {
	return spec.Runtime != nil && spec.Runtime.Stacked != nil && *spec.Runtime.Stacked
}

func calculateStepSize(stackDoc *kind.KindDoc) float64 {
	if a, ok := stackDoc.Metadata.Annotations["simulation"].(map[string]any); ok {
		if v, ok := a["stepsize"].(float64); ok {
			return v
		}
	}
	return 0.0005
}

func calculateEndTime(stackDoc *kind.KindDoc) float64 {
	if a, ok := stackDoc.Metadata.Annotations["simulation"].(map[string]any); ok {
		if v, ok := a["endtime"].(float64); ok {
			return v
		}
	}
	return 0.002
}
