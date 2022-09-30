/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package msgbus_test

import (
	"testing"
	"time"

	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/utils/testingutils"
)

var testStreamCfg = &nats.StreamConfig{
	Name:     "abc",
	Subjects: []string{"abc"},
	MaxAge:   2 * time.Minute,
	Replicas: 3,
	Storage:  nats.MemoryStorage,
}

func TestJetStream_PersistentSubscribeInterfaceAccuracy(t *testing.T) {
	sub := testStreamCfg.Name
	data := [][]byte{[]byte("123"), []byte("abc"), []byte("asdf")}

	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()
	js := msgbus.MustConnectJetStream(nc)
	s, err := msgbus.NewJetStreamStreamer(nc, js, testStreamCfg)
	require.NoError(t, err)

	// Publish data to the subject.
	for _, d := range data {
		require.NoError(t, s.Publish(sub, d))
	}

	ch1 := make(chan msgbus.Msg)
	pSub, err := s.PersistentSubscribe(sub, "indexer", func(m msgbus.Msg) {
		ch1 <- m
		require.NoError(t, m.Ack())
	})
	require.NoError(t, err)

	// Should receive all messages that were published.
	require.NoError(t, receiveExpectedUpdates(ch1, data))
	require.NoError(t, pSub.Close())

	// Make sure when we recreate the subscription, we don't receive new messages (all old ack messages should be ignored).
	ch2 := make(chan msgbus.Msg)
	pSub, err = s.PersistentSubscribe(sub, "indexer", func(m msgbus.Msg) {
		ch2 <- m
		require.NoError(t, m.Ack())
	})
	require.NoError(t, err)

	// Should receive no messages.
	require.NoError(t, receiveExpectedUpdates(ch2, [][]byte{}))
	require.NoError(t, pSub.Close())

	// New durable subscribe with a different name should receive all of the old updates.
	ch3 := make(chan msgbus.Msg)
	pSub, err = s.PersistentSubscribe(sub, "new_indexer", func(m msgbus.Msg) {
		ch3 <- m
		require.NoError(t, m.Ack())
	})
	require.NoError(t, err)

	// Should receive all messages on this channel.
	require.NoError(t, receiveExpectedUpdates(ch3, data))
	require.NoError(t, pSub.Close())
}

func TestJetStream_PersistentSubscribeMultiConsumer(t *testing.T) {
	sub := testStreamCfg.Name
	data := [][]byte{[]byte("123"), []byte("abc"), []byte("asdf")}

	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()
	js := msgbus.MustConnectJetStream(nc)
	s, err := msgbus.NewJetStreamStreamer(nc, js, testStreamCfg)
	require.NoError(t, err)

	// Publish data to the subject.
	for _, d := range data {
		require.NoError(t, s.Publish(sub, d))
	}

	ch1 := make(chan msgbus.Msg)
	pSub1, err := s.PersistentSubscribe(sub, "indexer", func(m msgbus.Msg) {
		ch1 <- m
		require.NoError(t, m.Ack())
	})
	require.NoError(t, err)

	ch2 := make(chan msgbus.Msg)
	pSub2, err := s.PersistentSubscribe(sub, "indexer", func(m msgbus.Msg) {
		ch2 <- m
		require.NoError(t, m.Ack())
	})
	require.NoError(t, err)

	var out [][]byte
	func() {
		for {
			select {
			case m := <-ch1:
				out = append(out, m.Data())
			case m := <-ch2:
				out = append(out, m.Data())
			case <-time.After(500 * time.Millisecond):
				return
			}
		}
	}()

	require.NoError(t, pSub1.Close())
	require.NoError(t, pSub2.Close())
	assert.ElementsMatch(t, data, out)

	ch3 := make(chan msgbus.Msg)
	pSub3, err := s.PersistentSubscribe(sub, "indexer", func(m msgbus.Msg) {
		ch3 <- m
		require.NoError(t, m.Ack())
	})
	require.NoError(t, err)
	require.NoError(t, s.Publish(sub, []byte("new_data_1")))
	require.NoError(t, s.Publish(sub, []byte("new_data_2")))
	// Should receive only new messages.
	require.NoError(t, receiveExpectedUpdates(ch3, [][]byte{[]byte("new_data_1"), []byte("new_data_2")}))
	require.NoError(t, pSub3.Close())
}

func TestJetStream_PublishAfterSubscribe(t *testing.T) {
	sub := testStreamCfg.Name
	data := [][]byte{[]byte("123"), []byte("abc"), []byte("asdf")}

	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()
	js := msgbus.MustConnectJetStream(nc)
	s, err := msgbus.NewJetStreamStreamer(nc, js, testStreamCfg)
	require.NoError(t, err)

	// Subscribe first to the data.
	ch1 := make(chan msgbus.Msg)
	pSub, err := s.PersistentSubscribe(sub, "indexer", func(m msgbus.Msg) {
		ch1 <- m
		require.NoError(t, m.Ack())
	})
	require.NoError(t, err)

	// Then publish data to the subject.
	for _, d := range data {
		require.NoError(t, s.Publish(sub, d))
	}

	// Should receive all messages that were published.
	require.NoError(t, receiveExpectedUpdates(ch1, data))
	require.NoError(t, pSub.Close())
}

func TestJetStream_PersistentSubscribeReattemptAck(t *testing.T) {
	sub := testStreamCfg.Name
	data := [][]byte{[]byte("123"), []byte("abc"), []byte("asdf")}

	// Test to make sure that not-acking a message will make sure that it comes back.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	js := msgbus.MustConnectJetStream(nc)

	ackWait := 100 * time.Millisecond
	s, err := msgbus.NewJetStreamStreamerWithConfig(nc, js, testStreamCfg, msgbus.JetStreamStreamerConfig{AckWait: ackWait})
	require.NoError(t, err)

	// Publish data to the subject.
	for _, d := range data {
		require.NoError(t, s.Publish(sub, d))
	}

	ch4 := make(chan msgbus.Msg)
	first := true
	pSub, err := s.PersistentSubscribe(sub, "indexer", func(m msgbus.Msg) {
		if first {
			first = false
			return
		}
		ch4 <- m
		require.NoError(t, m.Ack())
	})
	require.NoError(t, err)

	// Receive all but the first data point.
	require.NoError(t, receiveExpectedUpdates(ch4, data[1:]))

	time.Sleep(ackWait)

	// Receive the last missing datapoint.
	require.NoError(t, receiveExpectedUpdates(ch4, data[0:1]))

	require.NoError(t, pSub.Close())
}

func TestJetStream_PeekLatestMessage_NoElements(t *testing.T) {
	sub := testStreamCfg.Name

	// Test PeekLatestMessage when the stream does not have elements.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	js := msgbus.MustConnectJetStream(nc)
	s, err := msgbus.NewJetStreamStreamer(nc, js, testStreamCfg)
	require.NoError(t, err)

	// Notice that we don't publish any data, so peek should not work.
	m, err := s.PeekLatestMessage(sub)
	require.NoError(t, err)
	// Expect bottom of queue to be nil because no elements found.
	require.Nil(t, m)
}

func TestJetStream_PeekLatestMessage_MultiElements(t *testing.T) {
	sub := testStreamCfg.Name
	data := [][]byte{[]byte("123"), []byte("abc"), []byte("asdf")}

	// Test PeekLatestMessage to return the latest result.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()
	js := msgbus.MustConnectJetStream(nc)
	s, err := msgbus.NewJetStreamStreamer(nc, js, testStreamCfg)
	require.NoError(t, err)

	for _, d := range data {
		require.NoError(t, s.Publish(sub, d))
	}

	m, err := s.PeekLatestMessage(sub)
	require.NoError(t, err)
	require.NotNil(t, m)
	// Expect bottom of queue to be the last element we pushed.
	assert.Equal(t, []byte("asdf"), m.Data())

	require.NoError(t, s.Publish(sub, []byte("qwer")))
	m, err = s.PeekLatestMessage(sub)
	require.NoError(t, err)
	require.NotNil(t, m)
	// Expect bottom of queue to be the last element we pushed.
	assert.Equal(t, []byte("qwer"), m.Data())
}

func TestJetStream_MultiSubjectStream(t *testing.T) {
	sub := "abc"
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()
	js := msgbus.MustConnectJetStream(nc)
	s, err := msgbus.NewJetStreamStreamer(nc, js, &nats.StreamConfig{
		Name:     sub,
		Subjects: []string{"abc", "abc.*", "abc.*.*", "abc.*.*.*"},
		MaxAge:   time.Minute * 2,
	})
	require.NoError(t, err)

	// Should be able to publish and receive single nested.
	require.NoError(t, s.Publish("abc.blah", []byte("abc")))
	m0, err := s.PeekLatestMessage("abc.blah")
	require.NoError(t, err)
	require.NotNil(t, m0)
	assert.Equal(t, []byte("abc"), m0.Data())

	// Should be able to publish and receive double nested.
	require.NoError(t, s.Publish("abc.blah.blah", []byte("asdf")))
	m1, err := s.PeekLatestMessage("abc.blah.blah")
	require.NoError(t, err)
	require.NotNil(t, m1)
	assert.Equal(t, []byte("asdf"), m1.Data())

	// Should be able to publish and receive triple nested.
	require.NoError(t, s.Publish("abc.blah.blah.blah", []byte("bteg")))
	m2, err := s.PeekLatestMessage("abc.blah.blah.blah")
	require.NoError(t, err)
	require.NotNil(t, m2)
	assert.Equal(t, []byte("bteg"), m2.Data())

	// Not part of the stream.
	require.Error(t, s.Publish("abc.blah.blah.blah.blah", []byte("btegs")))
	m3, err := s.PeekLatestMessage("abc.blah.blah.blah.blah")
	assert.Error(t, err)
	assert.Nil(t, m3)
}
