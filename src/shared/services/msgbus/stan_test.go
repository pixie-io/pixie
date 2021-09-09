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
	"bytes"
	"errors"
	"fmt"
	"testing"
	"time"

	"github.com/nats-io/stan.go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/utils/testingutils"
)

func receiveExpectedUpdates(c <-chan msgbus.Msg, data [][]byte) error {
	// On some investigations, found that messages are sent < 500us, chose a
	// timeout that was 100x in case of any unexpected interruptions.
	timeout := 50 * time.Millisecond

	// We do not early return any errors, otherwise we won't consume all messages
	// sent by STAN and risk race conditions in tests.
	// If no errors are reached, `err` will be nil.
	var err error

	numUpdates := 0
	for numUpdates < len(data) {
		select {
		case m := <-c:
			if !bytes.Equal(data[numUpdates], m.Data()) {
				err = fmt.Errorf("Data doesn't match on update %d", numUpdates)
			}
			numUpdates++
		case <-time.After(timeout):
			err = errors.New("Timed out waiting for messages on subscription")
		}
	}
	run := true
	for run {
		select {
		case idxMessage := <-c:
			err = fmt.Errorf("Unexpected index message: %s", string(idxMessage.Data()))
		case <-time.After(timeout):
			run = false
		}
	}
	return err
}

type stanMessage struct {
	sm *stan.Msg
}

func (m *stanMessage) Data() []byte {
	return m.sm.Data
}
func (m *stanMessage) Ack() error {
	return m.sm.Ack()
}

func TestSTAN_receiveExpectedUpdatesHelper(t *testing.T) {
	// Make sure that the test helper receiveExpectedUpdates() will properly return an error if we don't expect any messages but messages come anyways.
	_, sc, cleanup := testingutils.MustStartTestStan(t, "stan", "test-client")
	defer cleanup()
	s, err := msgbus.NewSTANStreamer(sc)
	require.NoError(t, err)

	sub := "abc"
	data := [][]byte{[]byte("123"), []byte("abc"), []byte("asdf")}

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

	defer func() {
		require.NoError(t, pSub.Close())
	}()

	// We expect this to error out because we intentionally send a message over stan, but the following call claims to expect no messages should be received over stan.
	err = receiveExpectedUpdates(ch1, [][]byte{})
	require.Error(t, err)
	require.Regexp(t, "Unexpected index message", err.Error())
}

func TestSTANPersistentSubscribeInterface(t *testing.T) {
	_, sc, cleanup := testingutils.MustStartTestStan(t, "stan", "test-client")
	defer cleanup()
	s, err := msgbus.NewSTANStreamer(sc)
	require.NoError(t, err)

	sub := "abc"
	data := [][]byte{[]byte("123"), []byte("abc"), []byte("asdf")}

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

func TestSTANPublishAfterSubscribe(t *testing.T) {
	_, sc, cleanup := testingutils.MustStartTestStan(t, "stan", "test-client")
	defer cleanup()
	s, err := msgbus.NewSTANStreamer(sc)
	require.NoError(t, err)

	sub := "abc"
	data := [][]byte{[]byte("123"), []byte("abc"), []byte("asdf")}

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

func TestSTANPersistentSubscribeReattemptAck(t *testing.T) {
	// Test to make sure that not-acking a message will make sure that it comes back.
	_, sc, cleanup := testingutils.MustStartTestStan(t, "stan", "test-client")
	defer cleanup()

	ackWait := 1 * time.Second

	s, err := msgbus.NewSTANStreamerWithConfig(sc, msgbus.STANStreamerConfig{AckWait: ackWait})
	require.NoError(t, err)

	sub := "abc"
	data := [][]byte{[]byte("123"), []byte("abc"), []byte("asdf")}

	// Publish data to the subject.
	for _, d := range data {
		require.NoError(t, s.Publish(sub, d))
	}

	ch4 := make(chan msgbus.Msg)
	first := true
	pSub, err := s.PersistentSubscribe(sub, "indexer", func(m msgbus.Msg) {
		if !first {
			ch4 <- m
			require.NoError(t, m.Ack())
		}
		first = false
	})
	require.NoError(t, err)

	// Receive all but the first data point.
	require.NoError(t, receiveExpectedUpdates(ch4, data[1:]))

	// TODO(philkuz) This timeout is rather slow - should we add a suboption that reduces this for tests?
	time.Sleep(ackWait)

	// Receive the last missing datapoint.
	require.NoError(t, receiveExpectedUpdates(ch4, data[0:1]))

	require.NoError(t, pSub.Close())
}

func TestSTANPeekLatestMessage_NoElements(t *testing.T) {
	// Test PeekLatestMessage when the stream does not have elements.
	_, sc, cleanup := testingutils.MustStartTestStan(t, "stan", "test-client")
	defer cleanup()

	s, err := msgbus.NewSTANStreamer(sc)
	require.NoError(t, err)

	// Notice that we don't publish any data, so peek should not work.
	m, err := s.PeekLatestMessage("abc")
	require.NoError(t, err)
	// Expect bottom of queue to be nil because no elements found.
	require.Nil(t, m)
}

func TestSTANPeekLatestMessage_MultiElements(t *testing.T) {
	// Test PeekLatestMessage to return the latest result.
	_, sc, cleanup := testingutils.MustStartTestStan(t, "stan", "test-client")
	defer cleanup()
	s, err := msgbus.NewSTANStreamer(sc)
	require.NoError(t, err)

	sub := "abc"
	data := [][]byte{[]byte("123"), []byte("abc"), []byte("asdf")}

	for _, d := range data {
		require.NoError(t, s.Publish(sub, d))
	}

	m, err := s.PeekLatestMessage(sub)
	require.NoError(t, err)
	// Expect bottom of queue to be the last element we pushed.
	assert.Equal(t, data[2], m.Data())
}

func TestSTANSwitchDeliveryMethods(t *testing.T) {
	// Make sure that when we change delivery methods for a Durable Queue we don't get old data.
	_, sc, cleanup := testingutils.MustStartTestStan(t, "stan", "test-client")
	defer cleanup()
	s, err := msgbus.NewSTANStreamer(sc)
	require.NoError(t, err)

	sub := "abc"
	data := [][]byte{[]byte("123"), []byte("abc"), []byte("asdf")}
	persistentName := "indexer"

	ch1 := make(chan msgbus.Msg)

	// Create a q sub w/ default Delivery settings (start at new).
	qsub, err := sc.QueueSubscribe(sub,
		persistentName,
		func(m *stan.Msg) {
			ch1 <- &stanMessage{sm: m}
			require.NoError(t, m.Ack())
		},
		stan.DurableName(persistentName),
		stan.SetManualAckMode(),
	)
	require.NoError(t, err)
	for _, d := range data {
		require.NoError(t, s.Publish(sub, d))
	}
	require.NoError(t, receiveExpectedUpdates(ch1, data))
	require.NoError(t, qsub.Close())

	ch2 := make(chan msgbus.Msg)
	deliverAllSub, err := sc.QueueSubscribe(sub,
		persistentName,
		func(m *stan.Msg) {
			ch2 <- &stanMessage{sm: m}
			require.NoError(t, m.Ack())
		},
		stan.DeliverAllAvailable(),
		stan.DurableName(persistentName),
		stan.SetManualAckMode(),
	)
	require.NoError(t, err)

	// Should receive all messages that were published.
	require.NoError(t, receiveExpectedUpdates(ch2, [][]byte{}))
	require.NoError(t, deliverAllSub.Close())
}
