// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package chart

import (
	"flag"
	"fmt"
	"os"
	"path"
	"regexp"
	"slices"
	"strconv"
	"strings"

	"github.boschdevcloud.com/fsil/fsil.go/command"

	"github.com/elliotchance/orderedmap/v3"

	"github.com/go-echarts/go-echarts/v2/charts"
	"github.com/go-echarts/go-echarts/v2/opts"
	"github.com/go-echarts/snapshot-chromedp/render"
)

type ChartCommand struct {
	command.Command

	title      string
	conditions string
	inputFile  string
	outputFile string
}

func NewChartCommand(name string) *ChartCommand {
	c := &ChartCommand{
		Command: command.Command{
			Name:    name,
			FlagSet: flag.NewFlagSet(name, flag.ExitOnError),
		},
	}
	c.FlagSet().StringVar(&c.title, "title", "Benchmark", "chart title")
	c.FlagSet().StringVar(&c.conditions, "conditions", "Conditions (N/A)", "chart conditions")
	c.FlagSet().StringVar(&c.inputFile, "input", "", "path to captured console output of benchmark simulation")
	c.FlagSet().StringVar(&c.outputFile, "output", "", "path to write generated chart ()")
	return c
}

func (c ChartCommand) Name() string {
	return c.Command.Name
}

func (c ChartCommand) FlagSet() *flag.FlagSet {
	return c.Command.FlagSet
}

func (c *ChartCommand) Parse(args []string) error {
	err := c.FlagSet().Parse(args)
	if err != nil {
		return err
	}
	if len(c.outputFile) == 0 {
		c.outputFile = strings.TrimSuffix(c.inputFile, path.Ext(c.inputFile)) + ".png"
	}
	return nil
}

func (c *ChartCommand) Run() error {

	series, err := c.getSeries()
	if err != nil {
		return err
	}
	err = c.generateChart(series)
	if err != nil {
		return err
	}
	return nil
}

type chartSeries struct {
	axis  *orderedmap.OrderedMap[string, any]
	lines *orderedmap.OrderedMap[string, []float64]
}

func (c *ChartCommand) getSeries() (*chartSeries, error) {
	data, err := os.ReadFile(c.inputFile)
	if err != nil {
		return nil, err
	}

	raw_sample := orderedmap.NewOrderedMap[string, []float64]()
	re_b := regexp.MustCompile(`:::(.*):::`)
	matches := re_b.FindAllStringSubmatch(string(data), -1)
	for _, match := range matches {
		re_bb := regexp.MustCompile(`benchmark:(.*)::(.*)`)
		m_bb := re_bb.FindAllStringSubmatch(match[1], -1)
		for _, m := range m_bb {
			key := m[1]
			samples := strings.Split(m[2], `;`)
			if ok := raw_sample.Has(key); !ok {
				raw_sample.Set(key, []float64{})
			}
			s, _ := raw_sample.Get(key)
			v, _ := strconv.ParseFloat(samples[len(samples)-1], 64)
			raw_sample.Set(key, append(s, v))
		}

	}

	series := chartSeries{
		axis:  orderedmap.NewOrderedMap[string, any](),
		lines: orderedmap.NewOrderedMap[string, []float64](),
	}
	for key, samples := range raw_sample.AllFromFront() {
		s := strings.SplitN(key, `_`, 2)
		name, sample := s[0], s[1]
		value := func() float64 {
			var max float64
			for _, v := range samples {
				if v > max {
					max = v
				}
			}
			return max
		}()
		if ok := series.axis.Has(sample); !ok {
			series.axis.Set(sample, nil)
		}

		if ok := series.lines.Has(name); !ok {
			series.lines.Set(name, []float64{})
		}
		sx, _ := series.lines.Get(name)
		series.lines.Set(name, append(sx, value))
	}

	return &series, nil
}

func (c *ChartCommand) generateChart(series *chartSeries) error {
	genLineData := func(data []float64) []opts.LineData {
		items := make([]opts.LineData, 0)
		for i := 0; i < len(data); i++ {
			items = append(items, opts.LineData{Value: data[i]})
		}
		return items
	}

	line := charts.NewLine()
	line.SetGlobalOptions(
		charts.WithAnimation(false),
		charts.WithTitleOpts(opts.Title{
			Title:    c.title,
			Left:     "center",
			Subtitle: c.conditions,
		}),
		charts.WithLegendOpts(opts.Legend{Top: "bottom"}),
		charts.WithYAxisOpts(opts.YAxis{Name: "Rel. to Realtime"}),
		charts.WithXAxisOpts(opts.XAxis{
			Name:         "(total signals / signals per step / number models)",
			NameLocation: "center",
			NameGap:      30,
		}),
	)
	axis := line.SetXAxis(slices.Collect(series.axis.Keys()))
	for key, val := range series.lines.AllFromFront() {
		axis.AddSeries(key, genLineData(val)).SetSeriesOptions(
			charts.WithLabelOpts(opts.Label{
				Show: opts.Bool(true),
			}),
			charts.WithLineStyleOpts(opts.LineStyle{
				Width: 2,
			}),
			charts.WithLineChartOpts(opts.LineChart{
				Smooth: opts.Bool(true),
			}),
		)
	}

	fmt.Printf("writing chart: %s\n", c.outputFile)
	config := render.NewSnapshotConfig(line.RenderContent(), c.outputFile)
	config.KeepHtml = false
	err := render.MakeSnapshot(config)
	if err != nil {
		return err
	}
	return nil
}
