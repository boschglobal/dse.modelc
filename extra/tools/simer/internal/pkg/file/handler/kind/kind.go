// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package kind

import (
	"bytes"
	"fmt"
	"io"
	"log/slog"
	"os"

	"github.com/boschglobal/dse.schemas/code/go/dse/kind"
	"gopkg.in/yaml.v3"
)

type YamlKindHandler struct{}

func (h *YamlKindHandler) Detect(file string) any {
	decoder, err := func(file string) (*yaml.Decoder, error) {
		yamlFile, err := os.Open(file)
		if err != nil {
			return nil, err
		}
		defer yamlFile.Close()

		data, _ := io.ReadAll(yamlFile)
		decoder := yaml.NewDecoder(bytes.NewReader(data))
		return decoder, nil
	}(file)
	if err != nil {
		fmt.Println(err)
		return nil
	}
	docList := []KindDoc{}
	for {
		var doc KindDoc
		if err := decoder.Decode(&doc); err != nil {
			if err == io.EOF {
				break
			}
			slog.Debug(fmt.Sprintf("Decode error;  file=%s (%s)", file, err.Error()))
			continue
		}
		doc.File = file
		docList = append(docList, doc)
		slog.Debug(fmt.Sprintf("Handler:  yaml/kind=%s", doc.Kind))
	}

	if len(docList) != 0 {
		return docList
	}
	return nil
}

type KindSpec interface{}

type KindDoc struct {
	kind_id  int64
	File     string
	Kind     string `yaml:"kind"`
	Metadata struct {
		Name        string                 `yaml:"name"`
		Annotations map[string]interface{} `yaml:"annotations"`
		Labels      map[string]string      `yaml:"labels"`
	} `yaml:"metadata"`
	Spec interface{} `yaml:"-"`
}

func SetSpec(kd *KindDoc) error {
	switch kd.Kind {
	case "Model":
		kd.Spec = new(kind.ModelSpec)
	case "SignalGroup":
		kd.Spec = new(kind.SignalGroupSpec)
	case "Stack":
		kd.Spec = new(kind.StackSpec)
	default:
		return fmt.Errorf("unknown kind: %s", kd.Kind)
	}
	return nil
}

func (kd *KindDoc) UnmarshalYAML(n *yaml.Node) error {
	type KD KindDoc
	type T struct {
		*KD  `yaml:",inline"`
		Spec yaml.Node `yaml:"spec"`
	}
	// Create the combined struct (i.e. outer KindDoc + Spec) and then decode
	// the yaml node. Kd will be populated, and then T.Spec can be specifically
	// decoded into the kd.Spec based on the kind.
	obj := &T{KD: (*KD)(kd)}
	if err := n.Decode(obj); err != nil {
		return err
	}
	if err := SetSpec(kd); err != nil {
		return err
	}
	return obj.Spec.Decode(kd.Spec)
}
