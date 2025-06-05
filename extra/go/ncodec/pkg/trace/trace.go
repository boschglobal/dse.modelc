// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package trace

import (
	"fmt"
	"log/slog"
	"os"
	"strconv"
	"strings"

	"github.com/boschglobal/dse.modelc/extra/go/ncodec/pkg/message"
)

type Trace[T message.MessageType] interface {
	TraceRX(msg T)
	TraceTX(msg T)
}

func GetTraceEnv(envName string) (bool, map[uint32]bool) {
	envName = strings.ToUpper(envName)
	filter := os.Getenv(envName)
	if filter == "" {
		return false, nil
	}

	if filter == "*" {
		slog.Debug(fmt.Sprintf("    <wildcard> (all frames)"))
		return true, nil
	}

	Filter := make(map[uint32]bool)
	ids := strings.Split(filter, ",")
	for _, idStr := range ids {
		id, err := strconv.ParseInt(idStr, 0, 32)
		if err == nil && id > 0 {
			id32 := uint32(id)
			Filter[id32] = true
			slog.Debug(fmt.Sprintf("    %02x", id32))
		}
	}
	return false, Filter
}
