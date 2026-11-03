// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package inspect

import (
	"maps"
	"os"
	"path/filepath"
	"reflect"
	"slices"
	"strings"
	"testing"

	"github.com/boschglobal/dse.clib/extra/go/file/index"

	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/app/simer"
)

const (
	testSimbusPath = "simbus"
	testModelcPath = "modelc"
	testModelcX32  = "modelc32"
	testModelcI386 = "modelc32_i386"
)

// cmdSummary is the part of a Simer command which the inventory must reproduce.
// The inventory intentionally omits what only depends on Simer flags: stack selection,
// the YAML-only filter on runtime.files (non-stacked) and the NAME__KEY env prefix (stacked).
type cmdSummary struct {
	Name  string
	Prog  string
	Files []string
	Env   map[string]string
}

// The inventory must describe exactly the commands which Simer builds (default flags).
func TestInventoryMatchesSimer(t *testing.T) {
	sims := map[string]string{
		"lua":     "testdata/lua/sim",
		"fmi":     "testdata/fmi/sim",
		"gateway": "testdata/gateway/sim",
		"stacked": writeSimerFixture(t),
	}
	for name, dir := range sims {
		t.Run(name, func(t *testing.T) {
			dir, err := filepath.Abs(dir)
			if err != nil {
				t.Fatal(err)
			}
			stacks, err := Inventory{}.collect(NewInspect(dir))
			if err != nil {
				t.Fatal(err)
			}
			wantSimbus, wantModels := inventoryCommands(stacks)
			gotSimbus, gotModels := simerCommands(t, dir)

			if len(gotModels) == 0 {
				t.Fatal("simer built no model commands")
			}
			if name == "stacked" {
				// Second stack: gateway skipped, SimBus started. First stack: stacked, its SimBus ignored.
				var names []string
				for _, c := range gotModels {
					names = append(names, c.Name)
				}
				if !slices.Equal(names, []string{"plain", "a,gw"}) || gotSimbus == nil {
					t.Fatalf("unexpected simer commands: simbus=%v models=%v", gotSimbus, names)
				}
			}

			if !reflect.DeepEqual(wantSimbus, gotSimbus) {
				t.Errorf("SimBus command\ninventory: %+v\nsimer:     %+v", wantSimbus, gotSimbus)
			}
			if !reflect.DeepEqual(wantModels, gotModels) {
				t.Errorf("model commands\ninventory: %+v\nsimer:     %+v", wantModels, gotModels)
			}
		})
	}
}

// inventoryCommands derives, from the inventory, the commands Simer is expected to run.
func inventoryCommands(stacks []InventoryStack) (*cmdSummary, []cmdSummary) {
	var simbus *cmdSummary
	var models []cmdSummary
	prog := func(arch string, current string) string {
		switch arch {
		case "x32":
			return testModelcX32
		case "i386":
			return testModelcI386
		}
		return current
	}

	// Rules which depend on Simer flags or its command line are applied here, not in the inventory.
	envFor := func(s InventoryStack, m InventoryModel, prefix bool) map[string]string {
		env := map[string]string{}
		maps.Copy(env, s.Env)
		for k, v := range m.Env {
			if prefix {
				k = strings.ToUpper(m.Name + "__" + k)
			}
			env[k] = v
		}
		return env
	}
	yamlOnly := func(files []string) []string {
		return slices.DeleteFunc(slices.Clone(files), func(f string) bool {
			ext := filepath.Ext(f)
			return ext != ".yaml" && ext != ".yml"
		})
	}

	for _, s := range stacks {
		// Simer starts only the SimBus of the first stack which has one.
		if !s.SimBus.Loopback && simbus == nil {
			simbus = &cmdSummary{Name: "SimBus", Prog: testSimbusPath, Files: s.SimBus.Instance.Files, Env: envFor(s, s.SimBus.Instance, false)}
		}
		if s.Stacked {
			c := cmdSummary{Prog: testModelcPath, Env: map[string]string{}}
			var names, files []string
			for _, m := range s.Models {
				if m.Skipped != "" {
					continue
				}
				names = append(names, m.Name)
				files = append(files, m.Files...)
				c.Prog = prog(m.Arch, c.Prog)
				maps.Copy(c.Env, envFor(s, m, true))
			}
			c.Name = strings.Join(names, ",")
			c.Files = removeDuplicate(files)
			models = append(models, c)
			continue
		}
		for _, m := range s.Models {
			if m.Skipped != "" {
				continue
			}
			models = append(models, cmdSummary{Name: m.Name, Prog: prog(m.Arch, testModelcPath), Files: yamlOnly(m.Files), Env: envFor(s, m, false)})
		}
	}
	return simbus, models
}

// simerCommands runs Simer's own command builders on the simulation.
func simerCommands(t *testing.T, dir string) (*cmdSummary, []cmdSummary) {
	t.Helper()
	wd, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.Chdir(dir); err != nil {
		t.Fatal(err)
	}
	defer os.Chdir(wd)

	idx := index.NewYamlFileIndex()
	idx.Scan(".")
	flags := simer.Flags{}

	// The YAML files are the arguments following the last "--<flag> <value>" pair.
	files := func(args []string, lastFlag string) []string {
		i := slices.Index(args, lastFlag)
		if i < 0 {
			t.Fatalf("flag %s not found in %v", lastFlag, args)
		}
		return args[i+2:]
	}

	var simbus *cmdSummary
	if c := simer.SimbusCommand(idx, testSimbusPath, flags); c != nil {
		simbus = &cmdSummary{Name: c.Name, Prog: c.Prog, Files: files(c.Args, "--stepsize"), Env: nonNil(c.Env)}
	}
	var models []cmdSummary
	for _, c := range simer.ModelCommandList(idx, testModelcPath, testModelcX32, testModelcI386, flags) {
		models = append(models, cmdSummary{Name: c.Name, Prog: c.Prog, Files: files(c.Args, "--endtime"), Env: nonNil(c.Env)})
	}
	return simbus, models
}

func nonNil(m map[string]string) map[string]string {
	if m == nil {
		return map[string]string{}
	}
	return m
}

// writeSimerFixture covers Simer behaviours which the static testdata does not:
// stacked runtimes, a second SimBus, gateway/external models, x32 and stack env.
func writeSimerFixture(t *testing.T) string {
	t.Helper()
	dir := t.TempDir()
	files := map[string]string{
		"data/other.yaml": `---
kind: Stack
metadata:
  name: second
spec:
  models:
    - name: simbus
      model: {name: simbus}
      uid: 0
      runtime:
        env: {SIMBUS_LOGLEVEL: "3"}
        files: [data/extra.yaml]
    - name: gw
      model: {name: GwModel}
      uid: 1
    - name: plain
      model: {name: Plain}
      uid: 2
      runtime:
        env: {P: "1"}
        files: [model/plain/a.lua, model/plain/b.yaml]
        paths: [model/plain/dir]
`,
		"data/simulation.yaml": `---
kind: Stack
metadata:
  name: stk
spec:
  runtime:
    stacked: true
    env: {STACK_VAR: "1"}
  models:
    - name: simbus
      model: {name: simbus}
      uid: 0
    - name: a
      model: {name: ModelA}
      uid: 1
      runtime:
        env: {X: "1"}
        files: [model/a/a.lua, model/a/data.yaml]
        paths: [model/a/dir]
        x32: true
    - name: ext
      model: {name: ExtModel}
      uid: 2
      runtime: {external: true}
    - name: gw
      model: {name: GwModel}
      uid: 3
      runtime:
        env: {Y: "2"}
`,
		"data/models.yaml": `---
kind: Model
metadata:
  name: ModelA
spec:
  runtime:
    dynlib:
      - {os: linux, arch: amd64, path: lib/a.so}
---
kind: Model
metadata:
  name: GwModel
spec:
  runtime:
    gateway: {}
---
kind: Model
metadata:
  name: Plain
spec:
  runtime:
    dynlib:
      - {os: linux, arch: amd64, path: lib/plain.so}
`,
		"data/extra.yaml":           "---\nkind: SignalGroup\nmetadata: {name: extra}\nspec: {signals: []}\n",
		"model/a/a.lua":             "-- lua\n",
		"model/a/data.yaml":         "---\nkind: SignalGroup\nmetadata: {name: a}\nspec: {signals: []}\n",
		"model/a/dir/more.yaml":     "---\nkind: SignalGroup\nmetadata: {name: more}\nspec: {signals: []}\n",
		"model/plain/a.lua":         "-- lua\n",
		"model/plain/b.yaml":        "---\nkind: SignalGroup\nmetadata: {name: b}\nspec: {signals: []}\n",
		"model/plain/dir/c.yaml":    "---\nkind: SignalGroup\nmetadata: {name: c}\nspec: {signals: []}\n",
		"model/plain/dir/skip.json": "{}\n",
	}
	for name, content := range files {
		path := filepath.Join(dir, name)
		if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	return dir
}
