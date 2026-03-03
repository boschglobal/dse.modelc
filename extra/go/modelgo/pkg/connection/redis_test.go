// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

package connection

import (
	"slices"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestRedisConnection(t *testing.T) {

	r := RedisConnection{Uid: 42, Url: "redis://localhost:6379"}

	err := r.Connect([]string{})
	assert.Nil(t, err)
	assert.True(t, slices.ContainsFunc([]string{"6.", "7."}, func(p string) bool {
		return strings.HasPrefix(r.version, p)
	}))
	assert.Equal(t, "dse.model.42", r.endpoint.pull)
	assert.Equal(t, "dse.simbus", r.endpoint.push)
	assert.Equal(t, 60*time.Second, r.endpoint.recvTimeout)

	c := r.client.Ping(r.ctx)
	assert.Nil(t, c.Err())

	r.Disconnect()

}

func TestRedisNotifyMessage(t *testing.T) {
	r := RedisConnection{Uid: 42, Url: "redis://localhost:6379"}
	err := r.Connect([]string{})
	assert.Nil(t, err)
	r.client.FlushAll(r.ctx)

	// Test WaitMessage: push raw bytes directly to the model pull queue and receive them.
	r.client.LPush(r.ctx, r.endpoint.pull, []byte("hello world"))
	msg, err := r.WaitMessage(false)
	assert.Nil(t, err)
	assert.Equal(t, []byte("hello world"), msg)

	// Test SendMessage: verify it completes without error.
	err = r.SendMessage([]byte("hello notify"))
	assert.Nil(t, err)

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
	msg, err := r.WaitMessage(false)
	elapsed := time.Now()
	assert.WithinDuration(t, start.Add(3*time.Second), elapsed, 500*time.Millisecond)
	assert.Nil(t, msg)
	assert.NotNil(t, err)

	// Immediate timeout (1s).
	start = time.Now()
	msg, err = r.WaitMessage(true)
	elapsed = time.Now()
	assert.WithinDuration(t, start.Add(1*time.Second), elapsed, 500*time.Millisecond)
	assert.Nil(t, msg)
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
