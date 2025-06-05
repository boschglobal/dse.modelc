// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package connection

import (
	"context"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	// "github.com/testcontainers/testcontainers-go"
)

// func TestMain(m *testing.M) {
// 	ctx := context.Background()
// 	req := testcontainers.ContainerRequest{
// 		Image:        "redis:latest",
// 		ExposedPorts: []string{"6379/tcp"},
// 		WaitingFor:   wait.ForLog("Ready to accept connections"),
// 	}
// 	redisC, err := testcontainers.GenericContainer(ctx, testcontainers.GenericContainerRequest{
// 		ContainerRequest: req,
// 		Started:          true,
// 	})
// 	defer testcontainers.TerminateContainer(redisC)

// 	exitCode := m.Run()

// 	//testcontainers.CleanupContainer(m, redisC)
// 	//require.NoError(t, err)

// 	os.Exit(exitCode)
// }

func TestRedisConnection(t *testing.T) {

	r := RedisConnection{Uid: 42, Url: "redis://localhost:6379"}

	err := r.Connect([]string{})
	assert.Nil(t, err)
	assert.True(t, strings.HasPrefix(r.version, "6."))
	assert.Equal(t, "dse.model.42", r.endpoint.pull)
	assert.Equal(t, "dse.simbus", r.endpoint.push)
	assert.Equal(t, 60*time.Second, r.endpoint.recvTimeout)

	c := r.client.Ping(r.ctx)
	assert.Nil(t, c.Err())

	r.Disconnect()

}

func TestRedisChannelMessage(t *testing.T) {
	ctx := context.Background()
	r := RedisConnection{Uid: 42, Url: "redis://localhost:6379"}
	err := r.Connect([]string{})
	assert.Nil(t, err)
	r.client.FlushAll(r.ctx)

	r.SendMessage([]byte("hello world"), "foo")
	c := r.client.RPop(ctx, r.endpoint.push)
	assert.Nil(t, c.Err())
	r.client.LPush(ctx, r.endpoint.pull, c.Val())
	msg, channel, err := r.WaitMessage(false)
	assert.Nil(t, err)
	assert.Equal(t, "foo", channel)
	assert.Equal(t, []byte("hello world"), msg)

	r.Disconnect()
}

func TestRedisNotifyMessage(t *testing.T) {
	ctx := context.Background()
	r := RedisConnection{Uid: 42, Url: "redis://localhost:6379"}
	err := r.Connect([]string{})
	assert.Nil(t, err)
	r.client.FlushAll(r.ctx)

	r.SendMessage([]byte("hello world"), "")
	c := r.client.RPop(ctx, r.endpoint.push)
	assert.Nil(t, c.Err())
	r.client.LPush(ctx, r.endpoint.pull, c.Val())
	msg, channel, err := r.WaitMessage(false)
	assert.Nil(t, err)
	assert.Equal(t, "", channel)
	assert.Equal(t, []byte("hello world"), msg)

	r.Disconnect()
}

func TestRedisWaitTimeout(t *testing.T) {
	r := RedisConnection{Uid: 42, Url: "redis://localhost:6379", endpoint: RedisEndpoint{recvTimeout: 3 * time.Second}}
	err := r.Connect([]string{})
	assert.Nil(t, err)
	require.Equal(t, 3*time.Second, r.endpoint.recvTimeout)
	r.client.FlushAll(r.ctx)

	// Configured timeout (3s)
	start := time.Now()
	msg, channel, err := r.WaitMessage(false)
	elapsed := time.Now()
	assert.WithinDuration(t, start.Add(3*time.Second), elapsed, 500*time.Millisecond)
	assert.Nil(t, msg)
	assert.Equal(t, "", channel)
	assert.NotNil(t, err)

	// Immediate timeout (1s).
	start = time.Now()
	msg, channel, err = r.WaitMessage(true)
	elapsed = time.Now()
	assert.WithinDuration(t, start.Add(1*time.Second), elapsed, 500*time.Millisecond)
	assert.Nil(t, msg)
	assert.Equal(t, "", channel)
	assert.NotNil(t, err)

	// TODO check the error type

	r.Disconnect()
}

// Send, wait, peek

func TestRedisToken(t *testing.T) {
	r := RedisConnection{}
	for i := range int32(5) {
		assert.Equal(t, i+1, r.Token())
	}
}
