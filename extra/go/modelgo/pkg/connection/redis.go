// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package connection

import (
	"bytes"
	"context"
	"fmt"
	"log/slog"
	"os"
	"strconv"
	"time"

	"github.com/boschglobal/dse.modelc/extra/go/modelgo/pkg/errors"
	red "github.com/redis/go-redis/v9"
	"github.com/vmihailenco/msgpack/v5"
)

var (
	recvTimeout    = 60 // --timeout or stack/connection/timeout
	connectTimeout = 5  // REDIS_CONNECTION_TIMEOUT
)

type RedisEndpoint struct {
	pull        string
	push        string
	recvTimeout time.Duration
}
type RedisConnection struct {
	Uid uint32
	Url string

	endpoint RedisEndpoint

	ctx     context.Context
	client  *red.Client
	version string

	token int32
}

func (r *RedisConnection) Connect(channels []string) error {
	slog.Info(fmt.Sprintf("Redis: Connect: %s", r.Url))
	if r.Uid == 0 {
		return fmt.Errorf("UID not configured")
	}
	slog.Debug(fmt.Sprintf("Redis: UID: %d", r.Uid))
	r.endpoint.push = "dse.simbus"
	r.endpoint.pull = fmt.Sprintf("dse.model.%d", r.Uid)
	if r.endpoint.recvTimeout == 0 {
		timeout, err := strconv.Atoi(os.Getenv("SIMBUS_TIMEOUT"))
		if err != nil {
			timeout = recvTimeout
		}
		r.endpoint.recvTimeout = time.Duration(timeout) * time.Second
	}
	slog.Info(fmt.Sprintf("Redis: PULL: %s (Model/Gateway)", r.endpoint.pull))
	slog.Info(fmt.Sprintf("Redis: PUSH: %s (SimBus)", r.endpoint.push))

	r.ctx = context.Background()
	opt, err := red.ParseURL(r.Url)
	if err != nil {
		return err
	}
	// TODO REDIS_CONNECTION_TIMEOUT
	// opt.ReadTimeout = 5
	// opt.WriteTimeout = 5
	// opt.DialTimeout = 5
	r.client = red.NewClient(opt)

	c := r.client.InfoMap(r.ctx, "server")
	if c.Err() != nil {
		return err
	}
	r.version = c.Item("Server", "redis_version")
	slog.Info(fmt.Sprintf("Redis: Version: %s", r.version))

	return nil
}

func (r *RedisConnection) Disconnect() {
	slog.Info(fmt.Sprintf("Redis: Disconnect:"))
	r.client.Close()
	r.client = nil
}

func (r *RedisConnection) SendMessage(msg []byte, channel string) (err error) {
	encodeFbs := func() []byte {
		buf := new(bytes.Buffer)
		enc := msgpack.NewEncoder(buf)
		if len(channel) > 0 {
			enc.EncodeString("SBCH")
		} else {
			enc.EncodeString("SBNO")
		}
		enc.EncodeString(channel)
		enc.EncodeBytes(msg)
		return buf.Bytes()
	}
	d := encodeFbs()
	slog.Debug(fmt.Sprintf("Redis: LPUSH -> %s (%d bytes)", r.endpoint.push, len(d)))
	r.client.LPush(r.ctx, r.endpoint.push, d)
	return
}

func (r *RedisConnection) WaitMessage(immediate bool) (msg []byte, channel string, err error) {
	timeout := r.endpoint.recvTimeout
	if immediate {
		timeout = time.Second
	}
	slog.Debug(fmt.Sprintf("Redis: BRPOP <- %s (timeout=%v)", r.endpoint.pull, timeout))
	c := r.client.BRPop(r.ctx, timeout, r.endpoint.pull)
	if c.Err() != nil {
		return nil, "", errors.ErrConnTimeoutWait
	}
	if len(c.Val()) != 2 {
		return nil, "", errors.ErrConnRedisRespIncomplete
	}

	decodeFbs := func(buf []byte) {
		reader := bytes.NewReader(buf)
		dec := msgpack.NewDecoder(reader)
		_, err := dec.DecodeString() // message indicator
		if err != nil {
			return
		}
		channel, err = dec.DecodeString()
		if err != nil {
			return
		}
		msg, err = dec.DecodeBytes()
	}
	decodeFbs([]byte(c.Val()[1]))

	return
}

func (r *RedisConnection) PeekMessage() (msg []byte, channel string, err error) {
	err = fmt.Errorf("no message error")
	return
}

func (r *RedisConnection) Token() int32 {
	r.token += 1
	return r.token
}
