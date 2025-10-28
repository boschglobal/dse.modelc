// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package simulation

import (
	"flag"
	"fmt"

	"github.com/boschglobal/dse.clib/extra/go/command"
	"github.com/boschglobal/dse.clib/extra/go/command/util"
	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
)

type SimulationCommand struct {
	command.Command

	outputFile   string
	modelCount   int
	signalChange int
	stacked      bool
}

func NewSimulationCommand(name string) *SimulationCommand {
	c := &SimulationCommand{
		Command: command.Command{
			Name:    name,
			FlagSet: flag.NewFlagSet(name, flag.ExitOnError),
		},
	}
	c.FlagSet().StringVar(&c.outputFile, "output", "", "path to write generated simulation file")
	c.FlagSet().IntVar(&c.modelCount, "count", 5, "number of models to generate")
	c.FlagSet().IntVar(&c.signalChange, "change", 40, "number of signals to change per step")
	c.FlagSet().BoolVar(&c.stacked, "stacked", false, "generate a stacked simulation")
	return c
}

func (c SimulationCommand) Name() string {
	return c.Command.Name
}

func (c SimulationCommand) FlagSet() *flag.FlagSet {
	return c.Command.FlagSet
}

func (c *SimulationCommand) Parse(args []string) error {
	return c.FlagSet().Parse(args)
}

func (c *SimulationCommand) Run() error {

	models := []kind.ModelInstance{}
	models = append(models, kind.ModelInstance{
		Name: "simbus",
		Model: struct {
			Mcl *struct {
				Models []struct {
					Name string `yaml:"name"`
				} `yaml:"models"`
				Strategy string `yaml:"strategy"`
			} `yaml:"mcl,omitempty"`
			Name string `yaml:"name"`
		}{
			Name: "simbus",
		},
		Channels: &[]kind.Channel{
			kind.Channel{
				Name:               util.StringPtr("data_channel"),
				ExpectedModelCount: &c.modelCount,
			},
		},
	})

	for i := 1; i <= c.modelCount; i++ {
		model := kind.ModelInstance{
			Name: fmt.Sprintf("benchmark_inst_%d", i),
			Uid:  i,
			Model: struct {
				Mcl *struct {
					Models []struct {
						Name string `yaml:"name"`
					} `yaml:"models"`
					Strategy string `yaml:"strategy"`
				} `yaml:"mcl,omitempty"`
				Name string `yaml:"name"`
			}{
				Name: fmt.Sprintf("Benchmark"),
			},
			Channels: &[]kind.Channel{
				kind.Channel{
					Name:  util.StringPtr("data_channel"),
					Alias: util.StringPtr("data"),
				},
			},
			Runtime: &kind.ModelInstanceRuntime{
				Files: &[]string{
					"data/signal_group.yaml",
				},
				Env: &map[string]string{
					"MODEL_COUNT":   fmt.Sprintf("%d", c.modelCount),
					"MODEL_ID":      fmt.Sprintf("%d", i),
					"SIGNAL_CHANGE": fmt.Sprintf("%d", c.signalChange),
				},
			},
		}
		models = append(models, model)
	}

	stack := kind.Stack{
		Kind: "Stack",
		Metadata: &kind.ObjectMetadata{
			Name: util.StringPtr("benchmark_stack"),
		},
		Spec: kind.StackSpec{
			Runtime: &kind.StackRuntime{
				Stacked: &c.stacked,
			},
		},
	}
	configureConnection(&stack)
	stack.Spec.Models = &models
	if err := util.WriteYaml(&stack, c.outputFile, false); err != nil {
		return err
	}

	// Write an empty SimBus model.
	simbus := kind.Model{
		Kind: "Model",
		Metadata: &kind.ObjectMetadata{
			Name: util.StringPtr("simbus"),
		},
	}
	if err := util.WriteYaml(&simbus, c.outputFile, true); err != nil {
		return err
	}

	return nil
}

func configureConnection(stack *kind.Stack) {
	timeout := 60
	redisTransport := kind.StackSpecConnectionTransport0{
		Redis: kind.RedisConnection{
			Timeout: &timeout,
			Uri:     util.StringPtr("redis://localhost:6379"),
		},
	}
	transport := kind.StackSpec_Connection_Transport{}
	transport.FromStackSpecConnectionTransport0(redisTransport)
	connection := struct {
		Timeout   *string                              `yaml:"timeout,omitempty"`
		Transport *kind.StackSpec_Connection_Transport `yaml:"transport,omitempty"`
	}{
		Transport: &transport,
	}
	stack.Spec.Connection = &connection
}
