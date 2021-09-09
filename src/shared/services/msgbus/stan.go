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
	"time"

	"github.com/nats-io/nats.go"
	"github.com/nats-io/stan.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

func init() {
	pflag.String("stan_cluster", "pl-stan", "The cluster ID of the stan cluster.")
}

// MustConnectSTAN tries to connect to the STAN message bus.
func MustConnectSTAN(nc *nats.Conn, clientID string) stan.Conn {
	stanClusterID := viper.GetString("stan_cluster")

	sc, err := stan.Connect(stanClusterID, clientID, stan.NatsConn(nc),
		stan.SetConnectionLostHandler(func(_ stan.Conn, reason error) {
			log.WithError(reason).Fatal("STAN Connection lost")
		}))
	if err != nil {
		log.WithError(err).WithField("ClusterID", stanClusterID).Fatal("Failed to connect to STAN")
	}

	log.WithField("ClusterID", stanClusterID).Info("Connected to STAN")

	return sc
}

// persistentSTANSub implements msgbus.PersistentSub for STAN subscriptions.
type persistentSTANSub struct {
	sub stan.Subscription
}

func (u *persistentSTANSub) Close() error {
	// STAN Unsubcribe() removes any notion of durable. Instead we must Close().
	// If you want to add the functionality and the API does not have it, don't call it
	// Unsubscribe. Unsubscribe is incredibly confusing in the STAN API. Instead
	// it should be something like Destroy.
	return u.sub.Close()
}

// stanMessage implements msgbus.Msg interface for STAN messages.
type stanMessage struct {
	sm *stan.Msg
}

func (m *stanMessage) Data() []byte {
	return m.sm.Data
}
func (m *stanMessage) Ack() error {
	return m.sm.Ack()
}

func wrapSTANMsgHandler(cb MsgHandler) stan.MsgHandler {
	return func(m *stan.Msg) {
		cb(&stanMessage{sm: m})
	}
}

// stanStreamer implements the msgbus.Streamer interface.
type stanStreamer struct {
	sc      stan.Conn
	ackWait time.Duration
}

func (s *stanStreamer) PersistentSubscribe(subject, persistentName string, cb MsgHandler) (PersistentSub, error) {
	sub, err := s.sc.QueueSubscribe(subject,
		persistentName,
		wrapSTANMsgHandler(cb),
		stan.DurableName(persistentName),
		stan.SetManualAckMode(),
		stan.MaxInflight(50),
		stan.DeliverAllAvailable(),
		stan.AckWait(s.ackWait),
	)

	if err != nil {
		return nil, err
	}

	return &persistentSTANSub{sub: sub}, nil
}

func (s *stanStreamer) Publish(subject string, data []byte) error {
	return s.sc.Publish(subject, data)
}

// emptyQueueTimeout is the time we wait before we consider a queue to be empty.
const emptyQueueTimeout = 200 * time.Millisecond

func (s *stanStreamer) PeekLatestMessage(subject string) (Msg, error) {
	dataCh := make(chan *stan.Msg)
	sub, err := s.sc.Subscribe(subject, func(m *stan.Msg) {
		dataCh <- m
		// Don't ack this message, we only want to receive a single message for this sub.
	}, stan.StartWithLastReceived(), stan.SetManualAckMode())
	if err != nil {
		return nil, err
	}

	defer sub.Unsubscribe()

	// Once we receive data or timeout, we give up.
	select {
	case m, ok := <-dataCh:
		if ok {
			return &stanMessage{sm: m}, nil
		}
	case <-time.After(emptyQueueTimeout):
		// This means the queue is considered empty, and we return no error but no element.
		break
	}
	return nil, nil
}

// STANStreamerConfig contains options that can be set for a STAN Streamer.
type STANStreamerConfig struct {
	// AckWait is the duration to wait before Ack() is considered failed and STAN knows to resend the value.
	AckWait time.Duration
}

// DefaultSTANStreamerConfig are the default settings for the STAN streamer
var DefaultSTANStreamerConfig = STANStreamerConfig{
	AckWait: stan.DefaultAckWait,
}

// NewSTANStreamerWithConfig creates a new Streamer implemented using STAN with specific configuration.
func NewSTANStreamerWithConfig(sc stan.Conn, cfg STANStreamerConfig) (Streamer, error) {
	return &stanStreamer{
		sc:      sc,
		ackWait: cfg.AckWait,
	}, nil
}

// NewSTANStreamer creates a new Streamer implemented using STAN with default configuration.
func NewSTANStreamer(sc stan.Conn) (Streamer, error) {
	return NewSTANStreamerWithConfig(sc, DefaultSTANStreamerConfig)
}
