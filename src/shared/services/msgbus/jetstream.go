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

package msgbus

import (
	"fmt"
	"strings"
	"time"

	"github.com/cenkalti/backoff/v4"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

func init() {
	pflag.Int("jetstream_cluster_size", 5, "The number of JetStream replicas, used to configure stream replication.")
}

// MustConnectJetStream creates a new JetStream connection.
func MustConnectJetStream(nc *nats.Conn) nats.JetStreamContext {
	js, err := nc.JetStream(nats.PublishAsyncMaxPending(4096))
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to JetStream")
	}
	return js
}

// persistentJetStreamSub is a wrapper around the JetStream subscription that implements the PersistentSub interface.
type persistentJetStreamSub struct {
	*nats.Subscription
}

func (s *persistentJetStreamSub) Close() error {
	return s.Unsubscribe()
}

// natsMessage implements msgbus.Msg interface for JetStream messages.
type natsMessage struct {
	*nats.Msg
}

func (m *natsMessage) Data() []byte {
	return m.Msg.Data
}

func (m *natsMessage) Ack() error {
	return m.Msg.Ack()
}

func wrapJetStreamMessageHandler(cb MsgHandler) nats.MsgHandler {
	return func(m *nats.Msg) {
		cb(&natsMessage{Msg: m})
	}
}

// jetStreamStreamer is a Streamer implemented using JetStream.
type jetStreamStreamer struct {
	js nats.JetStreamContext

	stream  *nats.StreamInfo
	ackWait time.Duration

	bOpts *backoff.ExponentialBackOff
}

func (s *jetStreamStreamer) PersistentSubscribe(subject, persistentName string, cb MsgHandler) (PersistentSub, error) {
	// Manually lookup or create the consumer. Later we bind to the consumer on the subscription
	// so that the consumer isn't automoatically deleted when the last subscriber leaves and we
	// maintain our cursor.
	consumerName := fmt.Sprintf("%s|%s", strings.ReplaceAll(subject, ".", "_"), persistentName)
	consumer, err := s.js.ConsumerInfo(s.stream.Config.Name, consumerName)

	if consumer == nil || err != nil {
		// There are a handful of gotchas with some of these config options.
		// Since the stream may have many subjects (our streams typically have wildcards),
		// we need to set the FilterSubject to the subject we want.
		// See https://github.com/nats-io/nats.go/issues/822#issuecomment-919238393
		// We also need DeliverSubject to be set for this to be a push consumer.
		// See https://github.com/nats-io/nats.go/issues/822#issuecomment-919225624
		_, err = s.js.AddConsumer(s.stream.Config.Name, &nats.ConsumerConfig{
			Durable:        consumerName,
			FilterSubject:  subject,
			DeliverPolicy:  nats.DeliverAllPolicy,
			DeliverSubject: nats.NewInbox(),
			DeliverGroup:   persistentName,
			AckWait:        s.ackWait,
			AckPolicy:      nats.AckExplicitPolicy,
			MaxAckPending:  50,
		})
		if err != nil {
			return nil, err
		}
	}

	sub, err := s.js.QueueSubscribe(subject,
		persistentName,
		wrapJetStreamMessageHandler(cb),
		nats.Bind(s.stream.Config.Name, consumerName),
		nats.ManualAck(),
		nats.AckWait(s.ackWait),
		nats.DeliverAll(),
	)

	if err != nil {
		return nil, err
	}

	return &persistentJetStreamSub{Subscription: sub}, nil
}

func (s *jetStreamStreamer) Publish(subject string, data []byte) error {
	return backoff.Retry(func() error {
		pubFuture, err := s.js.PublishAsync(subject, data)
		if err != nil {
			return err
		}
		select {
		case <-pubFuture.Ok():
			return nil
		case err = <-pubFuture.Err():
			return err
		}
	}, s.bOpts)
}

func (s *jetStreamStreamer) PeekLatestMessage(subject string) (Msg, error) {
	dataCh := make(chan *nats.Msg)
	sub, err := s.js.Subscribe(subject, func(m *nats.Msg) {
		dataCh <- m
	}, nats.ManualAck(), nats.DeliverLast())
	if err != nil {
		return nil, err
	}

	defer sub.Unsubscribe()

	// Once we receive data or timeout, we give up.
	select {
	case m, ok := <-dataCh:
		if ok {
			return &natsMessage{Msg: m}, nil
		}
	case <-time.After(emptyQueueTimeout):
		// This means the queue is considered empty, and we return no error but no element.
		return nil, nil
	}
	return nil, nil
}

// JetStreamStreamerConfig contains options that can be set for a JetStream Streamer.
type JetStreamStreamerConfig struct {
	// AckWait is the duration to wait before Ack() is considered failed and JetStream knows to resend the value.
	AckWait time.Duration
}

// DefaultJetStreamStreamerConfig are the default settings for the JetStream streamer
var DefaultJetStreamStreamerConfig = JetStreamStreamerConfig{
	AckWait: 30 * time.Second,
}

// NewJetStreamStreamerWithConfig creates a new Streamer implemented using JetStream with specific configuration.
func NewJetStreamStreamerWithConfig(nc *nats.Conn, js nats.JetStreamContext, sCfg *nats.StreamConfig, cfg JetStreamStreamerConfig) (Streamer, error) {
	clusterSize := viper.GetInt("jetstream_cluster_size")
	if sCfg.Replicas > clusterSize {
		sCfg.Replicas = clusterSize
	}
	streamInfo, err := js.AddStream(sCfg)
	if err != nil {
		return nil, err
	}

	bOpts := backoff.NewExponentialBackOff()
	bOpts.InitialInterval = publishRetryInterval
	bOpts.MaxElapsedTime = publishTimeout

	return &jetStreamStreamer{
		js:      js,
		stream:  streamInfo,
		ackWait: cfg.AckWait,
		bOpts:   bOpts,
	}, nil
}

// NewJetStreamStreamer creates a new Streamer implemented using JetStream with default configuration.
func NewJetStreamStreamer(nc *nats.Conn, js nats.JetStreamContext, sCfg *nats.StreamConfig) (Streamer, error) {
	return NewJetStreamStreamerWithConfig(nc, js, sCfg, DefaultJetStreamStreamerConfig)
}
