// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"context"
	"io"
	"log"
	"log/slog"
	"os"
)

var (
	logLevel = new(slog.LevelVar)
	// 0 = slog.LevelDebug (-8)
	// 1 = slog.LevelDebug (-4)
	// 2 = slog.LevelInfo  (0)
	// 3 = slog.LevelWarn  (4)
	// 4 = slog.LevelError (8)
)

type PlainLogHandlerOptions struct {
	SlogOpts slog.HandlerOptions
}

type PlainLogHandler struct {
	slog.Handler
	l *log.Logger
}

func (h *PlainLogHandler) Handle(ctx context.Context, r slog.Record) error {
	level := r.Level.String() + ":"
	//timeString := r.Time.Format("[14:03:02.000]")
	h.l.Println(level, r.Message)
	return nil
}

func NewPlainLogHandler(out io.Writer, opts PlainLogHandlerOptions) *PlainLogHandler {
	h := &PlainLogHandler{
		Handler: slog.NewTextHandler(out, &opts.SlogOpts),
		l:       log.New(out, "", 0),
	}
	return h
}

func NewLogger(level int) *slog.Logger {
	logger := slog.New(NewPlainLogHandler(os.Stderr, PlainLogHandlerOptions{
		SlogOpts: slog.HandlerOptions{
			Level: logLevel,
		},
	}))
	switch {
	case level == 0:
		logLevel.Set(slog.LevelDebug)
	case level == 1:
		logLevel.Set(slog.LevelDebug)
	case level == 2:
		logLevel.Set(slog.LevelInfo)
	case level == 3:
		logLevel.Set(slog.LevelWarn)
	case level == 4:
		logLevel.Set(slog.LevelError)
	default:
		logLevel.Set(slog.LevelWarn)
	}
	return logger
}
