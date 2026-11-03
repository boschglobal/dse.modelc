// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package inspect

import (
	"context"
	"fmt"
	"os"
	"strings"

	"github.com/boschglobal/dse.clib/extra/go/file/handler/kind"
	"github.com/boschglobal/dse.clib/extra/go/file/index"
	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
	"gopkg.in/yaml.v3"
)

type Inventory struct {
}

type InventoryStack struct {
	Name        string
	File        string
	StepSize    float64
	EndTime     float64
	Transport   *InventoryTransport
	Stacked     bool
	Env         map[string]string // Stack spec.runtime.env.
	Annotations map[string]any
	SimBus      InventorySimBus
	Models      []InventoryModel
}

type InventoryTransport struct {
	Kind    string
	URI     string
	Timeout string
}

type InventorySimBus struct {
	// Loopback is true when no simbus model is present (loopback or remote).
	Loopback bool
	Instance InventoryModel
}

type InventoryChannel struct {
	Name               string
	Alias              string
	Selectors          map[string]string
	ExpectedModelCount int
}

type InventoryModel struct {
	Name     string // Instance name.
	Model    string // Model (kind) name.
	UID      int
	MCL      string // Lua (stack runtime.mcl) or adapter and version from the Model doc.
	Skipped  string // Why Simer does not start it (external, gateway).
	Stacked  bool   // Runs inside the single stacked ModelC process.
	Arch     string // Non-default ModelC executable (x32, i386).
	Files    []string
	Env      map[string]string
	Channels []InventoryChannel
	// Runtime rows are {parameter, value} from the Model doc spec.runtime (dynlib, executable, mcl).
	Runtime [][]string
	// Annotations are from the stack spec.models[]; ModelAnnotations from the Model doc.
	Annotations      map[string]any
	ModelAnnotations map[string]any
}

func (inv Inventory) Name() string {
	return "Simulation Inventory"
}

func (inv Inventory) Function(ctx context.Context, ins *Inspect) TaskResult {
	stacks, err := inv.collect(ins)
	if err != nil {
		return TaskResult{TargetName: inv.Name(), Err: err}
	}

	details := map[string]any{}
	for _, s := range stacks {
		details["Stack: "+s.Name] = Markdown(renderStack(s))
	}
	return TaskResult{
		TargetName: inv.Name(),
		Details:    details,
	}
}

func (inv Inventory) collect(ins *Inspect) ([]InventoryStack, error) {
	idx := index.NewYamlFileIndex()
	idx.Scan(ins.simPath)
	if len(idx.FileMap) == 0 {
		return nil, fmt.Errorf("no files indexed in %q", ins.simPath)
	}

	var stacks []InventoryStack
	for _, stackDoc := range idx.DocMap["Stack"] {
		stacks = append(stacks, inv.stack(ins, idx, stackDoc))
	}
	return stacks, nil
}

// stack mirrors the traversal of SimbusCommand() and ModelCommandList().
func (inv Inventory) stack(ins *Inspect, idx *index.YamlFileIndex, stackDoc kind.KindDoc) InventoryStack {
	spec := stackDoc.Spec.(*schema_kind.StackSpec)
	raw := readRawStack(stackDoc.File, stackDoc.Metadata.Name)
	s := InventoryStack{
		Name:        stackDoc.Metadata.Name,
		File:        relPath(ins.simPath, stackDoc.File),
		StepSize:    calculateStepSize(&stackDoc),
		EndTime:     calculateEndTime(&stackDoc),
		Transport:   raw.transport(),
		Stacked:     isRuntimeStacked(spec),
		Env:         stackEnv(spec),
		Annotations: stackDoc.Metadata.Annotations,
	}

	// SimBus: only the first simbus model is used (see SimbusCommand()).
	s.SimBus.Loopback = true
	if spec.Models != nil {
		for _, m := range *spec.Models {
			if m.Name != "simbus" {
				continue
			}
			s.SimBus.Loopback = false
			files := []string{s.File}
			if m.Runtime != nil && m.Runtime.Files != nil {
				files = append(files, *m.Runtime.Files...)
			}
			s.SimBus.Instance = InventoryModel{
				Name: m.Name, Model: m.Model.Name, UID: m.Uid, MCL: raw.mcl(m.Name),
				Files: removeDuplicate(files), Env: modelEnv(&m),
				Channels: channelsOf(m.Channels), Annotations: annotationsOf(m.Annotations),
			}
			break
		}
	}

	// Models: mirrors ModelCommandList() and stackedModelCmd().
	if spec.Models != nil {
		for _, m := range *spec.Models {
			if m.Name == "simbus" {
				continue
			}
			im := inv.model(ins, idx, stackDoc, m, s.Stacked)
			if lua := raw.mcl(m.Name); lua != "" {
				im.MCL = lua
			}
			s.Models = append(s.Models, im)
		}
	}
	return s
}

func (inv Inventory) model(ins *Inspect, idx *index.YamlFileIndex, stackDoc kind.KindDoc, m schema_kind.ModelInstance, stacked bool) InventoryModel {
	im := InventoryModel{Name: m.Name, Model: m.Model.Name, UID: m.Uid, Stacked: stacked, Env: modelEnv(&m), Channels: channelsOf(m.Channels), Annotations: annotationsOf(m.Annotations)}
	if m.Runtime != nil && m.Runtime.External != nil && *m.Runtime.External {
		im.Skipped = "external"
		return im
	}
	if m.Runtime != nil && m.Runtime.X32 != nil && *m.Runtime.X32 {
		im.Arch = "x32"
	} else if m.Runtime != nil && m.Runtime.I386 != nil && *m.Runtime.I386 {
		im.Arch = "i386"
	}

	files := []string{relPath(ins.simPath, stackDoc.File)}
	if doc, ok := findModelDoc(idx, m.Model.Name); ok {
		modelSpec := doc.Spec.(*schema_kind.ModelSpec)
		im.ModelAnnotations = doc.Metadata.Annotations
		if modelSpec.Runtime != nil && modelSpec.Runtime.Mcl != nil && len(*modelSpec.Runtime.Mcl) > 0 {
			im.MCL = mclName(doc.Metadata.Annotations)
		}
		// Only the non-stacked path skips gateway models (stackedModelCmd() does not).
		if !stacked && modelSpec.Runtime != nil && modelSpec.Runtime.Gateway != nil {
			im.Skipped = "gateway"
			return im
		}
		im.Runtime = runtimeRows(modelSpec)
		files = append(files, relPath(ins.simPath, doc.File))
	}
	if m.Runtime != nil && m.Runtime.Files != nil {
		files = append(files, *m.Runtime.Files...)
	}
	if m.Runtime != nil && m.Runtime.Paths != nil {
		files = append(files, listFiles(ins.simPath, *m.Runtime.Paths, []string{".yaml", ".yml"})...)
	}
	im.Files = removeDuplicate(files)
	return im
}

// runtimeRows flattens spec.runtime into {parameter, value} rows, e.g. {"dynlib.linux.amd64", "`path`"}.
func runtimeRows(spec *schema_kind.ModelSpec) [][]string {
	if spec.Runtime == nil {
		return nil
	}
	key := func(kind string, qualifiers ...*string) string {
		parts := []string{kind}
		for _, q := range qualifiers {
			if q != nil && *q != "" {
				parts = append(parts, *q)
			}
		}
		return strings.Join(parts, ".")
	}
	var rows [][]string
	libs := func(kind string, specs *[]schema_kind.LibrarySpec) {
		if specs == nil {
			return
		}
		for _, l := range *specs {
			rows = append(rows, []string{key(kind, l.Os, l.Arch, l.Variant), codeSpan(l.Path)})
		}
	}

	libs("dynlib", spec.Runtime.Dynlib)
	if spec.Runtime.Executable != nil {
		for _, e := range *spec.Runtime.Executable {
			var l []string
			if e.Libs != nil {
				for _, p := range *e.Libs {
					l = append(l, codeSpan(p))
				}
			}
			rows = append(rows, []string{key("executable", e.Os, e.Arch), strings.Join(l, ", ")})
		}
	}
	libs("mcl", spec.Runtime.Mcl)
	return rows
}

func annotationsOf(a *schema_kind.Annotations) map[string]any {
	if a == nil {
		return nil
	}
	return *a
}

// mclName returns "<adapter> <version>" from the Model annotations, or a description of what is missing.
func mclName(ann map[string]any) string {
	adapter, _ := ann["mcl_adapter"].(string)
	version, _ := ann["mcl_version"].(string)
	var missing []string
	if adapter == "" {
		missing = append(missing, "mcl_adapter")
	}
	if version == "" {
		missing = append(missing, "mcl_version")
	}
	if len(missing) > 0 {
		return "not configured correctly (missing annotation: " + strings.Join(missing, ", ") + ")"
	}
	return adapter + " " + version
}

func channelsOf(chs *[]schema_kind.Channel) []InventoryChannel {
	if chs == nil {
		return nil
	}
	var out []InventoryChannel
	for _, c := range *chs {
		ic := InventoryChannel{}
		if c.Name != nil {
			ic.Name = *c.Name
		}
		if c.Alias != nil {
			ic.Alias = *c.Alias
		}
		if c.ExpectedModelCount != nil {
			ic.ExpectedModelCount = *c.ExpectedModelCount
		}
		if c.Selectors != nil {
			ic.Selectors = *c.Selectors
		}
		out = append(out, ic)
	}
	return out
}

// rawStack holds fields the schema types do not expose: the transport union is
// not populated by YAML decoding and runtime.mcl is not in the instance schema.
type rawStack struct {
	Spec struct {
		Connection struct {
			Transport map[string]map[string]any `yaml:"transport"`
		} `yaml:"connection"`
		Models []struct {
			Name    string `yaml:"name"`
			Runtime struct {
				MCL any `yaml:"mcl"`
			} `yaml:"runtime"`
		} `yaml:"models"`
	} `yaml:"spec"`
}

func readRawStack(file, stackName string) rawStack {
	f, err := os.Open(file)
	if err != nil {
		return rawStack{}
	}
	defer f.Close()

	dec := yaml.NewDecoder(f)
	for {
		var doc struct {
			Kind     string `yaml:"kind"`
			Metadata struct {
				Name string `yaml:"name"`
			} `yaml:"metadata"`
			rawStack `yaml:",inline"`
		}
		if err := dec.Decode(&doc); err != nil {
			return rawStack{}
		}
		if doc.Kind == "Stack" && doc.Metadata.Name == stackName {
			return doc.rawStack
		}
	}
}

func (r rawStack) transport() *InventoryTransport {
	for kind, v := range r.Spec.Connection.Transport {
		t := &InventoryTransport{Kind: kind}
		if uri, ok := v["uri"].(string); ok {
			t.URI = uri
		}
		if timeout, ok := v["timeout"]; ok {
			t.Timeout = fmt.Sprint(timeout)
		}
		return t
	}
	return nil
}

func (r rawStack) mcl(model string) string {
	for _, m := range r.Spec.Models {
		if m.Name == model && m.Runtime.MCL != nil {
			return fmt.Sprint(m.Runtime.MCL)
		}
	}
	return ""
}
