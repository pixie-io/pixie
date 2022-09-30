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
)

// emptyQueueTimeout is the time we wait before we consider a queue to be empty.
const emptyQueueTimeout = 200 * time.Millisecond
const publishRetryInterval = 25 * time.Millisecond
const publishTimeout = 1 * time.Minute

// V2CDurableStream is the stream config for Durable v2c messages.
var V2CDurableStream = &nats.StreamConfig{
	Name: "V2CStream",
	Subjects: []string{
		"v2c.*.*.*",
	},
	MaxAge:   15 * time.Minute,
	Replicas: 5,
}

// MetadataIndexStream is the stream config for MetadataIndex messages.
var MetadataIndexStream = &nats.StreamConfig{
	Name: "MetadataIndexStream",
	Subjects: []string{
		"MetadataIndex.*",
	},
	MaxAge:   24 * time.Hour,
	Replicas: 5,
}

// Msg is the interface for a message sent over the stream
type Msg interface {
	// Data returns the serialized data stored in the message.
	Data() []byte
	// Ack acknowledges the message.
	Ack() error
}

// MsgHandler is a function that processes Msg.
type MsgHandler func(msg Msg)

// PersistentSub is the interface to an active persistent subscription.
type PersistentSub interface {
	// Close the subscription, but allow future PersistentSubs to read from the sub starting after
	// the last acked message.
	Close() error
}

// Streamer is an interface for any streaming handler.
type Streamer interface {
	// PersistentSubscribe creates a persistent subscription on a subject, calling the message
	// handler callback on each message that arrives on the sub.
	//
	// Here persistence means that if the subscription closes or dies and later resumes,
	// the Subscription will continue from the earliest message that was not acked.
	//
	// This position in the stream will be tracked according to the (subject, persistentName) pair.
	// * If you need a new subscription to see all of the available stream messages, you can receive them
	//   by invoking PersistentSubscribe() on the same subject but a new persistentName.
	// * If you call PersistentSubscribe() with a new subject but an existing persistentName, the implementation
	//   should treat it as a new persistent subscription and send all data available on the subscription.
	//
	// Parallel callers of PersistentSubscribe that use the same subject + persistentName pair will be added
	// to the same WorkQueue: messages published on that subject will be assigned to one of
	// the callers. If the assigned caller does not Ack() a message within an implementation's
	// timeout, then the message will be reassigned to another worker.
	PersistentSubscribe(subject, persistentName string, cb MsgHandler) (PersistentSub, error)

	// Publish publishes the data to the specific subject.
	Publish(subject string, data []byte) error

	// PeekLatestMessage returns the last message published on a subject. If no messages
	// exist for the subject method returns `nil`.
	//
	// PeekLatestMessage does not care about the state of any Sub. It strictly returns the last message sent from a Publish()
	// call.
	PeekLatestMessage(subject string) (Msg, error)
}
