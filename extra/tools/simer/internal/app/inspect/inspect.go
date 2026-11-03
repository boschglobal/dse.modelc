// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package inspect

import (
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"sort"
	"strings"
)

// Markdown is a Details value which the text printer emits as-is (unfenced).
type Markdown string

type TaskResult struct {
	TargetName string
	Details    map[string]any
	Err        error
}

type InspectTask interface {
	Name() string
	Function(ctx context.Context, ins *Inspect) TaskResult
}

type ResultPrinter func(r TaskResult)

type Inspect struct {
	simPath string
	printer ResultPrinter

	tasks []InspectTask
}

type InspectOption func(*Inspect)

func WithTask(task InspectTask) InspectOption {
	return func(ins *Inspect) {
		ins.tasks = append(ins.tasks, task)
	}
}

func WithResultPrinter(p ResultPrinter) InspectOption {
	return func(ins *Inspect) {
		ins.printer = p
	}
}

func NewInspect(simPath string, opts ...InspectOption) *Inspect {
	ins := &Inspect{
		simPath: simPath,
		printer: NewTextPrinter(os.Stdout),
		tasks:   []InspectTask{},
	}
	for _, opt := range opts {
		opt(ins)
	}
	return ins
}

func (ins *Inspect) Run(ctx context.Context) error {
	var errs []error
	for _, task := range ins.tasks {
		r := task.Function(ctx, ins)
		if r.TargetName == "" {
			r.TargetName = task.Name()
		}
		if r.Err != nil {
			errs = append(errs, fmt.Errorf("%s: %w", task.Name(), r.Err))
		}
		ins.printer(r)
	}
	return errors.Join(errs...)
}

func NewTextPrinter(w io.Writer) ResultPrinter {
	printed := false
	return func(r TaskResult) {
		if printed {
			fmt.Fprintln(w) // Separate from the previous result.
		}
		printed = true
		fmt.Fprintf(w, "# %s\n", r.TargetName)
		if r.Err != nil {
			writeSection(w, "error", "text", r.Err.Error()+"\n")
		}
		keys := make([]string, 0, len(r.Details))
		for k := range r.Details {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		for _, k := range keys {
			body, lang := "", "text"
			switch v := r.Details[k].(type) {
			case Markdown:
				fmt.Fprintf(w, "\n## %s\n\n%s\n", k, strings.TrimSpace(string(v)))
				continue
			case []string:
				body = strings.Join(v, "\n") + "\n"
			case string:
				body = v + "\n"
			default:
				body = fmt.Sprintf("%v\n", v)
			}
			writeSection(w, k, lang, body)
		}
	}
}

// writeSection emits an H2 and a fenced block.
func writeSection(w io.Writer, title, lang, body string) {
	fence := strings.Repeat("`", max(3, longestBacktickRun(body)+1))
	fmt.Fprintf(w, "\n## %s\n\n%s%s\n%s%s\n", title, fence, lang, body, fence)
}

func longestBacktickRun(s string) int {
	longest, run := 0, 0
	for _, c := range s {
		if c == '`' {
			run++
			longest = max(longest, run)
		} else {
			run = 0
		}
	}
	return longest
}
