// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package simer

import (
	"fmt"
	"log/slog"
	"slices"
	"strconv"
	"strings"

	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/pkg/file/handler"
	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/pkg/file/handler/kind"
	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/pkg/index"
	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/pkg/session"

	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

func scanFiles(exts []string) []string {
	files := []string{}
	for _, ext := range exts {
		indexed_files, _ := index.IndexFiles(".", ext)
		files = append(files, indexed_files...)
	}

	return files
}

func loadKindDocs(files []string) ([]kind.KindDoc, error) {
	var docList []kind.KindDoc
	for _, f := range files {
		//println(f)
		_, docs, err := handler.ParseFile(f)
		if err != nil {
			slog.Debug(fmt.Sprintf("Parse failed (%s) on file: %s", err.Error(), f))
			continue
		}
		dl := docs.([]kind.KindDoc)
		docList = append(docList, dl...)
	}
	return docList, nil
}

func IndexYamlFiles(path string) map[string][]kind.KindDoc {
	files := scanFiles([]string{".yml", ".yaml"})
	kindDocs, err := loadKindDocs(files)
	if err != nil {
		slog.Error(err.Error())
	}

	index := map[string][]kind.KindDoc{}

	slog.Info(fmt.Sprintf("Kind Docs (YAML):"))
	for _, doc := range kindDocs {
		slog.Info(fmt.Sprintf("kind: %s; name=%s (%s)", doc.Kind, doc.Metadata.Name, doc.File))

		index[doc.Kind] = append(index[doc.Kind], doc)
	}

	return index
}

func RedisCommand(redisPath string) *session.Command {
	if redisPath == "" {
		slog.Info("No Redis path specified, remote operation(?).")
		return nil
	}
	slog.Debug("Build Redis Server command...")
	return &session.Command{
		Name:       "Redis",
		Prog:       redisPath,
		Args:       []string{"/usr/local/etc/redis/redis.conf"},
		Quiet:      true,
		KillNoWait: true,
	}
}

func SimbusCommand(docMap map[string][]kind.KindDoc, simbusPath string, flags Flags) *session.Command {
	if simbusPath == "" {
		slog.Info("No SimBus path specified, remote operation(?).")
		return nil
	}
	slog.Debug("Build SimBus command...")
	if stackDocs, ok := docMap["Stack"]; ok {
		for _, stackDoc := range stackDocs {
			slog.Debug(stackDoc.File)
			slog.Debug(fmt.Sprintf("  %s", stackDoc.Metadata.Name))
			// All or only some stacks?
			if flags.Stack != "" {
				if slices.Contains(flags.StackList, stackDoc.Metadata.Name) == false {
					continue
				}
			}
			stackSpec := stackDoc.Spec.(*schema_kind.StackSpec)
			// Search for the SimBus model.
			for _, model := range *stackSpec.Models {
				slog.Debug(fmt.Sprintf("    %s", model.Name))
				if model.Name != "simbus" {
					continue
				}
				// YAML files.
				yamlFiles := []string{stackDoc.File}
				if model.Runtime != nil && model.Runtime.Files != nil {
					yamlFiles = append(yamlFiles, *model.Runtime.Files...)
				}
				// Arguments.
				args := []string{}
				//args = append(args, "--help")
				//args = append(args, "--logger", "2")
				if flags.Transport != "" {
					args = append(args, "--transport", flags.Transport)
				}
				if flags.Uri != "" {
					args = append(args, "--uri", flags.Uri)
				}
				args = append(args, "--stepsize", strconv.FormatFloat(flags.StepSize, 'f', -1, 64))
				if flags.Timeout > 0.0 {
					args = append(args, "--timeout", strconv.FormatFloat(flags.Timeout, 'f', -1, 64))
				}
				cmd := session.Command{
					Name: "SimBus",
					Prog: simbusPath,
					Args: append(args, yamlFiles...),
					Env:  calculateEnv(stackSpec, &model, flags),
				}
				slog.Info(fmt.Sprintf("SimBus: stack=%s (%s)", stackDoc.Metadata.Name, stackDoc.File))
				slog.Info(fmt.Sprintf("  Prog : %s", cmd.Prog))
				slog.Info(fmt.Sprintf("  Args : %s", cmd.Args))
				slog.Info(fmt.Sprintf("  Env  : %s", cmd.Env))
				return &cmd
			}
		}
	}
	slog.Warn("No SimBus model found in stack/simulation, remote operation(?).")
	return nil
}

func findModelDoc(docMap map[string][]kind.KindDoc, modelName string) (*kind.KindDoc, bool) {
	if modelDocs, ok := docMap["Model"]; ok {
		for _, modelDoc := range modelDocs {
			if modelDoc.Metadata.Name == modelName {
				return &modelDoc, true
			}
		}
	}
	return nil, false
}

func ModelCommandList(docMap map[string][]kind.KindDoc, modelcPath string, modelcX32Path string, flags Flags) []*session.Command {
	cmds := []*session.Command{}

	slog.Debug("Build Model commands...")
	if stackDocs, ok := docMap["Stack"]; ok {
		for _, stackDoc := range stackDocs {
			slog.Debug(stackDoc.File)
			slog.Debug(fmt.Sprintf("  %s", stackDoc.Metadata.Name))
			// All or only some stacks?
			if flags.Stack != "" {
				if slices.Contains(flags.StackList, stackDoc.Metadata.Name) == false {
					continue // Next stack.
				}
			}
			stackSpec := stackDoc.Spec.(*schema_kind.StackSpec)
			// Stacked is a special case, emit/return the single command.
			if stackSpec.Runtime != nil && stackSpec.Runtime.Stacked != nil && *stackSpec.Runtime.Stacked {
				slog.Debug(("Build stacked command"))
				cmd := stackedModelCmd(&stackDoc, modelcPath, modelcX32Path, flags, docMap)
				cmds = append(cmds, cmd)
				continue // Next stack.
			}
			// Generate commands for each model in this stack.
			for _, model := range *stackSpec.Models {
				if model.Name == "simbus" {
					continue // Next model.
				}
				slog.Debug(fmt.Sprintf("    %s (%s)", model.Name, model.Model.Name))
				// Locate the YAML files.
				yamlFiles := []string{stackDoc.File}
				if doc, ok := findModelDoc(docMap, model.Model.Name); ok {
					modelSpec := doc.Spec.(*schema_kind.ModelSpec)
					if modelSpec.Runtime.Gateway != nil {
						slog.Debug(fmt.Sprintf("Gateway model: %s  (start externally)", model.Name))
						continue // Gateway model, skip (started externally).
					}
					yamlFiles = append(yamlFiles, doc.File)
				}
				if model.Runtime != nil && model.Runtime.Files != nil {
					yamlFiles = append(yamlFiles, *model.Runtime.Files...)
				}
				// Run as 32bit process?
				progPath := modelcPath
				if model.Runtime != nil && model.Runtime.X32 != nil && *model.Runtime.X32 {
					progPath = modelcX32Path
				}
				cmd := buildModelCmd(model.Name, progPath, yamlFiles, calculateEnv(stackSpec, &model, flags), flags)
				slog.Info(fmt.Sprintf("Model: name=%s, stack=%s", model.Name, stackDoc.Metadata.Name))
				slog.Info(fmt.Sprintf("  Model : %s", model.Model.Name))
				slog.Info(fmt.Sprintf("  Prog  : %s", cmd.Prog))
				slog.Info(fmt.Sprintf("  Args  : %s", cmd.Args))
				slog.Info(fmt.Sprintf("  Env   : %s", cmd.Env))
				cmds = append(cmds, cmd)
			}
		}
	}
	return cmds
}

func stackedModelCmd(stackDoc *kind.KindDoc, modelcPath string, modelcX32Path string, flags Flags, docMap map[string][]kind.KindDoc) *session.Command {
	stackSpec := stackDoc.Spec.(*schema_kind.StackSpec)
	instanceName := []string{}
	modelName := []string{}
	yamlFiles := []string{stackDoc.File}
	progPath := modelcPath
	env := map[string]string{}

	for _, model := range *stackSpec.Models {
		if model.Name == "simbus" {
			continue
		}
		slog.Debug(fmt.Sprintf("    %s (%s)", model.Name, model.Model.Name))
		instanceName = append(instanceName, model.Name)
		modelName = append(modelName, model.Model.Name)
		// Locate the YAML files.
		if doc, ok := findModelDoc(docMap, model.Model.Name); ok {
			yamlFiles = append(yamlFiles, doc.File)
		}
		if model.Runtime != nil && model.Runtime.Files != nil {
			yamlFiles = append(yamlFiles, *model.Runtime.Files...)
		}
		// Run as 32bit process?
		if model.Runtime != nil && model.Runtime.X32 != nil && *model.Runtime.X32 {
			progPath = modelcX32Path
		}
		// Collate the environment.
		modelEnv := calculateEnv(stackSpec, &model, flags)
		for k, v := range modelEnv {
			env[k] = v
		}
	}
	cmd := buildModelCmd(strings.Join(instanceName, ","), progPath, yamlFiles, env, flags)
	slog.Info(fmt.Sprintf("Model Stack: stack=%s", stackDoc.Metadata.Name))
	slog.Info(fmt.Sprintf("  Names : %s", strings.Join(instanceName, ",")))
	slog.Info(fmt.Sprintf("  Model : %s", strings.Join(modelName, ",")))
	slog.Info(fmt.Sprintf("  Prog  : %s", cmd.Prog))
	slog.Info(fmt.Sprintf("  Args  : %s", cmd.Args))
	slog.Info(fmt.Sprintf("  Env   : %s", cmd.Env))
	return cmd
}

func removeDuplicate[T comparable](s []T) []T {
	seen := make(map[T]struct{}, len(s))
	return slices.DeleteFunc(s, func(val T) bool {
		_, dup := seen[val]
		seen[val] = struct{}{}
		return dup
	})
}

func buildModelCmd(name string, path string, yamlFiles []string, env map[string]string, flags Flags) *session.Command {

	yamlFiles = removeDuplicate(yamlFiles)
	args := []string{}

	// Command modifiers.
	if len(flags.Gdb) != 0 {
		if slices.Contains(strings.Split(name, ","), flags.Gdb) == true {
			// Run model 'name' via gdbserver.
			args = append(args, "localhost:2159", path)
			path = "/usr/bin/gdbserver"
		}
	} else if len(flags.Valgrind) != 0 {
		if slices.Contains(strings.Split(name, ","), flags.Valgrind) == true {
			// Prefix 'path' with the valgrind command and options.
			valgrindArgs := []string{
				"--track-origins=yes",
				"--leak-check=full",
				"--error-exitcode=808",
			}
			args = append(args, valgrindArgs...)
			args = append(args, path)
			path = "/usr/bin/valgrind"
		}
	}

	// Continue building the command.
	args = append(args, "--name", name)
	if flags.Transport != "" {
		args = append(args, "--transport", flags.Transport)
	}
	if flags.Uri != "" {
		args = append(args, "--uri", flags.Uri)
	}
	args = append(args, "--stepsize", strconv.FormatFloat(flags.StepSize, 'f', -1, 64))
	args = append(args, "--endtime", strconv.FormatFloat(flags.EndTime, 'f', -1, 64))
	if flags.Timeout > 0.0 {
		args = append(args, "--timeout", strconv.FormatFloat(flags.Timeout, 'f', -1, 64))
	}
	cmd := func() *session.Command {
		c := session.Command{
			Name: name,
			Prog: path,
			Args: append(args, yamlFiles...),
			Env:  env,
		}
		return &c
	}()
	return cmd
}

func calculateEnv(stack *schema_kind.StackSpec, model *schema_kind.ModelInstance, flags Flags) map[string]string {
	env := map[string]string{}
	if stack.Runtime != nil && stack.Runtime.Env != nil {
		for k, v := range *stack.Runtime.Env {
			env[k] = v
		}
	}
	if model != nil && model.Runtime != nil && model.Runtime.Env != nil {
		for k, v := range *model.Runtime.Env {
			env[k] = v
		}
	}
	// Apply modifications (format MODEL:NAME=VAL).
	for _, mod := range flags.EnvModifier {
		m := strings.SplitN(mod, "=", 2)
		if len(m) != 2 {
			continue
		}
		k := strings.SplitN(m[0], ":", 2)
		if len(k) != 2 {
			continue
		}
		if model.Name == k[0] {
			env[k[1]] = m[1]
		}
	}
	return env
}
