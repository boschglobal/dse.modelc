// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package handler

import (
	"fmt"
	"log/slog"
	"path/filepath"
	"strings"

	"github.com/boschglobal/dse.modelc/extra/docker/simer/simer/internal/pkg/file/handler/kind"
)

type Handler interface {
	Detect(file string) any
}

func ParseFile(file string) (Handler, any, error) {
	var fileType string = "application/octet-stream"
	slog.Debug(fmt.Sprintf("Parse file: %s ...", file))

	yamlExtensions := map[string]bool{
		".yaml": true,
		".yml":  true,
	}
	if yamlExtensions[strings.ToLower(filepath.Ext(file))] {
		fileType = "text/yaml"
	}
	slog.Debug(fmt.Sprintf("Filetype: %s", fileType))

	fileHandlers := map[string][]Handler{
		"text/yaml": {
			&kind.YamlKindHandler{},
		},
	}
	handler, data, err := func(file string, handlers []Handler) (Handler, any, error) {
		for _, h := range handlers {
			data := h.Detect(file)
			if data != nil {
				return h, data, nil
			}
		}
		return nil, nil, fmt.Errorf("unsupported file")
	}(file, fileHandlers[fileType])

	// Return the selected handler and parsed doc objects.
	if err != nil {
		slog.Debug(fmt.Sprintf("No handler located"))
		return nil, nil, err
	}
	return handler, data, nil
}
