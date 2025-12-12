// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package simer

import (
	"fmt"
	"io/fs"
	"log/slog"
	"os"
	"path/filepath"
	"slices"
	"strconv"
	"strings"

	"github.com/boschglobal/dse.clib/extra/go/file/handler/kind"
	"github.com/boschglobal/dse.clib/extra/go/file/index"

	"github.com/boschglobal/dse.modelc/extra/tools/simer/internal/pkg/session"

	schema_kind "github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

func listFiles(paths []string, exts []string) []string {
	files := []string{}
	for _, path := range paths {
		fileSystem := os.DirFS(".")
		fs.WalkDir(fileSystem, path, func(s string, d fs.DirEntry, e error) error {
			slog.Debug(fmt.Sprintf("ListFiles: %s (%t, %s)", s, d.IsDir(), filepath.Ext(s)))
			if !d.IsDir() && slices.Contains(exts, filepath.Ext(s)) {
				files = append(files, s)
			}
			return nil
		})
	}

	return files
}

func RedisCommand(redisPath string, quiet bool) *session.Command {
	if redisPath == "" {
		slog.Info("No Redis path specified, remote operation(?).")
		return nil
	}
	slog.Debug("Build Redis Server command...")
	return &session.Command{
		Name:       "Redis",
		Prog:       redisPath,
		Args:       []string{"/usr/local/etc/redis/redis.conf"},
		Quiet:      quiet,
		KillNoWait: true,
	}
}

func SimbusCommand(index *index.YamlFileIndex, simbusPath string, flags Flags) *session.Command {
	if simbusPath == "" {
		slog.Info("No SimBus path specified; loopback or remote SimBus connection.")
		return nil
	}
	slog.Debug("Build SimBus command...")
	if stackDocs, ok := index.DocMap["Stack"]; ok {
		for _, stackDoc := range stackDocs {
			slog.Debug(stackDoc.File)
			slog.Debug(fmt.Sprintf("  %s", stackDoc.Metadata.Name))
			// All or only some stacks?
			if flags.Stack != "" {
				if !slices.Contains(flags.StackList, stackDoc.Metadata.Name) {
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
				// Base command.
				yamlFiles = removeDuplicate(yamlFiles)
				path, args := processCmdModifiers("simbus", simbusPath, flags)
				// Arguments.
				//args = append(args, "--help")
				//args = append(args, "--logger", "2")
				if flags.Transport != "" {
					args = append(args, "--transport", flags.Transport)
				}
				if flags.Uri != "" {
					args = append(args, "--uri", flags.Uri)
				}
				args = append(args, "--stepsize", calculateStepSize(flags, &stackDoc))
				if flags.Timeout > 0.0 {
					args = append(args, "--timeout", strconv.FormatFloat(flags.Timeout, 'f', -1, 64))
				}
				cmd := session.Command{
					Name: "SimBus",
					Prog: path,
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

func findModelDoc(index *index.YamlFileIndex, modelName string) (*kind.KindDoc, bool) {
	if modelDocs, ok := index.DocMap["Model"]; ok {
		for _, modelDoc := range modelDocs {
			if modelDoc.Metadata.Name == modelName {
				return &modelDoc, true
			}
		}
	}
	return nil, false
}

func ModelCommandList(index *index.YamlFileIndex, modelcPath string, modelcX32Path string, modelcI386Path string, flags Flags) []*session.Command {
	cmds := []*session.Command{}

	slog.Debug("Build Model commands...")
	if stackDocs, ok := index.DocMap["Stack"]; ok {
		for _, stackDoc := range stackDocs {
			slog.Debug(stackDoc.File)
			slog.Debug(fmt.Sprintf("  %s", stackDoc.Metadata.Name))
			// All or only some stacks?
			if flags.Stack != "" {
				if !slices.Contains(flags.StackList, stackDoc.Metadata.Name) {
					continue // Next stack.
				}
			}
			stackSpec := stackDoc.Spec.(*schema_kind.StackSpec)
			// Stacked is a special case, emit/return the single command.
			if stackSpec.Runtime != nil && stackSpec.Runtime.Stacked != nil && *stackSpec.Runtime.Stacked {
				slog.Debug(("Build stacked command"))
				cmd := stackedModelCmd(&stackDoc, modelcPath, modelcX32Path, modelcI386Path, flags, index)
				cmds = append(cmds, cmd)
				continue // Next stack.
			}
			// Generate commands for each model in this stack.
			for _, model := range *stackSpec.Models {
				if model.Name == "simbus" {
					continue // Next model.
				}
				slog.Debug(fmt.Sprintf("    %s (%s)", model.Name, model.Model.Name))
				if model.Runtime != nil && model.Runtime.External != nil && *model.Runtime.External {
					slog.Debug(fmt.Sprintf("External model: %s  (start externally)", model.Name))
					continue // External model, skip (started externally).
				}
				// Locate the YAML files.
				yamlFiles := []string{stackDoc.File}
				if doc, ok := findModelDoc(index, model.Model.Name); ok {
					modelSpec := doc.Spec.(*schema_kind.ModelSpec)
					if modelSpec.Runtime.Gateway != nil {
						slog.Debug(fmt.Sprintf("Gateway model: %s  (start externally)", model.Name))
						continue // Gateway model, skip (started externally).
					}
					yamlFiles = append(yamlFiles, doc.File)
				}
				if model.Runtime != nil && model.Runtime.Files != nil {
					yamlFiles = append(yamlFiles, listFiles(*model.Runtime.Files, []string{".yaml", ".yml"})...)
				}
				if model.Runtime != nil && model.Runtime.Paths != nil {
					yamlFiles = append(yamlFiles, listFiles(*model.Runtime.Paths, []string{".yaml", ".yml"})...)
				}
				// Run as 32bit process?
				progPath := modelcPath
				if model.Runtime != nil && model.Runtime.X32 != nil && *model.Runtime.X32 {
					progPath = modelcX32Path
				} else if model.Runtime != nil && model.Runtime.I386 != nil && *model.Runtime.I386 {
					progPath = modelcI386Path
				}
				cmd := buildModelCmd(model.Name, progPath, yamlFiles, calculateEnv(stackSpec, &model, flags), flags, &stackDoc)
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

func stackedModelCmd(stackDoc *kind.KindDoc, modelcPath string, modelcX32Path string, modelcI386Path string, flags Flags, index *index.YamlFileIndex) *session.Command {
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
		if model.Runtime != nil && model.Runtime.External != nil && *model.Runtime.External {
			slog.Debug(fmt.Sprintf("External model: %s  (start externally)", model.Name))
			continue // External model, skip (started externally).
		}
		instanceName = append(instanceName, model.Name)
		modelName = append(modelName, model.Model.Name)
		// Locate the YAML files.
		if doc, ok := findModelDoc(index, model.Model.Name); ok {
			yamlFiles = append(yamlFiles, doc.File)
		}
		if model.Runtime != nil && model.Runtime.Files != nil {
			yamlFiles = append(yamlFiles, *model.Runtime.Files...)
		}
		if model.Runtime != nil && model.Runtime.Paths != nil {
			yamlFiles = append(yamlFiles, listFiles(*model.Runtime.Paths, []string{".yaml", ".yml"})...)
		}
		// Run as 32bit process?
		if model.Runtime != nil && model.Runtime.X32 != nil && *model.Runtime.X32 {
			progPath = modelcX32Path
		} else if model.Runtime != nil && model.Runtime.I386 != nil && *model.Runtime.I386 {
			progPath = modelcI386Path
		}
		// Collate the environment.
		modelEnv := calculateEnv(stackSpec, &model, flags)
		for k, v := range modelEnv {
			env[k] = v
		}
	}
	cmd := buildModelCmd(strings.Join(instanceName, ","), progPath, yamlFiles, env, flags, stackDoc)
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

func processCmdModifiers(name string, path string, flags Flags) (string, []string) {
	args := []string{}
	if len(flags.Gdb) != 0 {
		if slices.Contains(strings.Split(name, ","), flags.Gdb) {
			// Prefix 'path' with the GDB command and options.
			gdbArgs := []string{
				"--eval-command=run",
				"--args",
			}
			args = append(args, gdbArgs...)
			args = append(args, path)
			path = "/usr/bin/gdb"
		}
	} else if len(flags.GdbServer) != 0 {
		if slices.Contains(strings.Split(name, ","), flags.GdbServer) {
			// Run model 'name' via gdbserver.
			args = append(args, "localhost:2159", path)
			path = "/usr/bin/gdbserver"
		}
	} else if len(flags.Valgrind) != 0 {
		if slices.Contains(strings.Split(name, ","), flags.Valgrind) {
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
	return path, args
}

func calculateStepSize(flags Flags, stackDoc *kind.KindDoc) string {
	if flags.StepSize > 0.0 {
		// User provided flag has priority.
		return strconv.FormatFloat(flags.StepSize, 'f', -1, 64)
	} else {
		// Otherwise, search for stack annotations.
		if stackDoc != nil {
			if simAnnotations, ok := stackDoc.Metadata.Annotations["simulation"].(map[string]any); ok {
				if stepSize, ok := simAnnotations["stepsize"].(float64); ok {
					return strconv.FormatFloat(stepSize, 'f', -1, 64)
				}
			}
		}
	}
	// Else, return the default value.
	return "0.0005"
}

func calculateEndTime(flags Flags, stackDoc *kind.KindDoc) string {
	if flags.EndTime > 0.0 {
		// User provided flag has priority.
		return strconv.FormatFloat(flags.EndTime, 'f', -1, 64)
	} else {
		// Otherwise, search for stack annotations.
		if stackDoc != nil {
			if simAnnotations, ok := stackDoc.Metadata.Annotations["simulation"].(map[string]any); ok {
				if endTime, ok := simAnnotations["endtime"].(float64); ok {
					return strconv.FormatFloat(endTime, 'f', -1, 64)
				}
			}
		}
	}
	return "0.002"
}

func buildModelCmd(name string, path string, yamlFiles []string, env map[string]string, flags Flags, stackDoc *kind.KindDoc) *session.Command {

	yamlFiles = removeDuplicate(yamlFiles)
	path, args := processCmdModifiers(name, path, flags)

	// Continue building the command.
	args = append(args, "--name", name)
	if flags.Transport != "" {
		args = append(args, "--transport", flags.Transport)
	}
	if flags.Uri != "" {
		args = append(args, "--uri", flags.Uri)
	}
	args = append(args, "--stepsize", calculateStepSize(flags, stackDoc))
	args = append(args, "--endtime", calculateEndTime(flags, stackDoc))
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
			if stack.Runtime != nil && stack.Runtime.Stacked != nil && *stack.Runtime.Stacked {
				if model.Name == "simbus" {
					// For simbus, directly set the var.
					env[k] = v
				} else {
					// Stacked, set with name prefix.
					key := strings.ToUpper(fmt.Sprintf("%s__%s", model.Name, k))
					env[key] = v
				}
			} else {
				// Set the var.
				env[k] = v
			}
		}
	}
	// Apply modifications (format MODEL:NAME=VAL).
	for _, mod := range flags.EnvModifier {
		m := strings.SplitN(mod, "=", 2)
		if len(m) != 2 {
			continue
		}
		k := strings.SplitN(m[0], ":", 2)
		switch len(k) {
		case 1:
			// Apply ENVAR.
			env[k[0]] = m[1]
		case 2:
			// Named ENVAR, apply to instance or stack.
			if model.Name != k[0] {
				continue
			}
			if stack.Runtime != nil && stack.Runtime.Stacked != nil && *stack.Runtime.Stacked {
				if model.Name == "simbus" {
					// For simbus, directly set the var.
					env[k[1]] = m[1]
				} else {
					// Stacked, set with name prefix.
					key := strings.ToUpper(fmt.Sprintf("%s__%s", model.Name, k[1]))
					env[key] = m[1]
				}
			} else {
				// Set the var.
				env[k[1]] = m[1]
			}
		}
	}
	return env
}
