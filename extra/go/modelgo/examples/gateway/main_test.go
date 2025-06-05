// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

//go:build test_e2e
// +build test_e2e

package main

import (
	"fmt"
	"os"
	"path"
	"runtime"
	"testing"

	"github.com/rogpeppe/go-internal/testscript"
)

func StringPtr(v string) *string {
	if len(v) > 0 {
		return &v
	} else {
		return nil
	}
}

func getCwd() string {
	_, filename, _, _ := runtime.Caller(0)
	return path.Join(path.Dir(filename))
}

func TestMain(m *testing.M) {
	os.Exit(testscript.RunMain(m, map[string]func() int{
		//		"gateway": main_,
	}))
}

func TestE2E(t *testing.T) {
	cwd := getCwd()
	runDir := path.Join(cwd, "..", "..")

	testscript.Run(t, testscript.Params{
		Dir: "testdata/script",
		Setup: func(e *testscript.Env) error {
			var vars = []string{
				fmt.Sprintf("BUILDDIR=%s", runDir),
				fmt.Sprintf("REPODIR=%s", runDir),
				fmt.Sprintf("WORKDIR=%s", os.Getenv("WORKDIR")),
			}
			e.Vars = append(e.Vars, vars...)
			return nil
		},
	})
}
