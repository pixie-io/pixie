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

package k8smeta

import (
	"fmt"
	"math"
	"sync"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/utils/messagebus"
)

var (
	// MetadataRequestSubscribeTopic is the channel which the listener is subscribed to for metadata requests.
	MetadataRequestSubscribeTopic = messagebus.C2VTopic("MetadataRequest")
	// metadataResponseTopic is the channel which the listener uses to responds to metadata requests.
	metadataResponseTopic = "MetadataResponse"
	// MissingMetadataRequestTopic is the channel which the listener should listen to missing metadata update requests on.
	MissingMetadataRequestTopic = "MissingMetadataRequests"
)

const batchSize = 128

// SendMessageFn is the function the TopicListener uses to publish messages back to NATS.
type SendMessageFn func(string, []byte) error

// MetadataTopicListener is responsible for listening to and handling messages on the metadata update topic.
type MetadataTopicListener struct {
	sendMessage SendMessageFn
	newMh       *Handler
	msgCh       chan *nats.Msg

	once   sync.Once
	quitCh chan struct{}
}

// NewMetadataTopicListener creates a new metadata topic listener.
func NewMetadataTopicListener(newMdHandler *Handler, sendMsgFn SendMessageFn) (*MetadataTopicListener, error) {
	m := &MetadataTopicListener{
		sendMessage: sendMsgFn,
		newMh:       newMdHandler,
		msgCh:       make(chan *nats.Msg, 1000),
		quitCh:      make(chan struct{}),
	}

	go m.processMessages()
	return m, nil
}

// Initialize handles any setup that needs to be done.
func (m *MetadataTopicListener) Initialize() error {
	return nil
}

// HandleMessage handles a message on the agent topic.
func (m *MetadataTopicListener) HandleMessage(msg *nats.Msg) error {
	m.msgCh <- msg
	return nil
}

// ProcessMessages processes the metadata requests.
func (m *MetadataTopicListener) processMessages() {
	for {
		select {
		case msg := <-m.msgCh:
			err := m.ProcessMessage(msg)
			if err != nil {
				log.WithError(err).Error("Failed to process metadata message")
			}
		case <-m.quitCh:
			log.Info("Received quit, stopping metadata listener")
			return
		}
	}
}

// Stop stops processing any metadata messagespb.
func (m *MetadataTopicListener) Stop() {
	m.once.Do(func() {
		close(m.quitCh)
	})
}

// ProcessMessage processes a single message in the metadata topic.
func (m *MetadataTopicListener) ProcessMessage(msg *nats.Msg) error {
	switch sub := msg.Subject; sub {
	case MissingMetadataRequestTopic:
		return m.processAgentMessage(msg)
	case MetadataRequestSubscribeTopic:
		return m.processCloudMessage(msg)
	default:
	}
	return nil
}

func (m *MetadataTopicListener) processAgentMessage(msg *nats.Msg) error {
	vzMsg := &messagespb.VizierMessage{}
	err := proto.Unmarshal(msg.Data, vzMsg)
	if err != nil {
		return err
	}
	if vzMsg.GetK8SMetadataMessage() == nil {
		return nil
	}
	req := vzMsg.GetK8SMetadataMessage().GetMissingK8SMetadataRequest()
	if req == nil {
		return nil
	}

	batches, firstAvailable, lastAvailable, err := m.getUpdatesInBatches(req.FromUpdateVersion, req.ToUpdateVersion, req.Selector)
	if err != nil {
		return err
	}

	for _, batch := range batches {
		resp := &messagespb.VizierMessage{
			Msg: &messagespb.VizierMessage_K8SMetadataMessage{
				K8SMetadataMessage: &messagespb.K8SMetadataMessage{
					Msg: &messagespb.K8SMetadataMessage_MissingK8SMetadataResponse{
						MissingK8SMetadataResponse: &metadatapb.MissingK8SMetadataResponse{
							Updates:              batch,
							FirstUpdateAvailable: firstAvailable,
							LastUpdateAvailable:  lastAvailable,
						},
					},
				},
			},
		}
		b, err := resp.Marshal()
		if err != nil {
			return err
		}

		err = m.sendMessage(getK8sUpdateChannel(req.Selector), b)
		if err != nil {
			return err
		}
	}

	return nil
}

func (m *MetadataTopicListener) getUpdatesInBatches(from int64, to int64, selector string) ([][]*metadatapb.ResourceUpdate, int64, int64, error) {
	updates, err := m.newMh.GetUpdatesForIP(selector, from, to)
	if err != nil {
		return nil, 0, 0, err
	}

	if len(updates) == 0 {
		return nil, 0, 0, nil
	}

	// Send updates in batches.
	batches := make([][]*metadatapb.ResourceUpdate, 0)
	batch := 0
	for batch*batchSize < len(updates) {
		batchSlice := updates[batch*batchSize : int(math.Min(float64((batch+1)*batchSize), float64(len(updates))))]
		batches = append(batches, batchSlice)
		batch++
	}

	var firstAvailable int64
	var lastAvailable int64

	// We are guaranteed to have at least one batch, since we exited when len(updates) == 0.
	firstAvailable = batches[0][0].UpdateVersion
	lastBatch := batches[len(batches)-1]
	lastAvailable = lastBatch[len(lastBatch)-1].UpdateVersion

	return batches, firstAvailable, lastAvailable, nil
}

func (m *MetadataTopicListener) processCloudMessage(msg *nats.Msg) error {
	c2vMsg := &cvmsgspb.C2VMessage{}
	err := proto.Unmarshal(msg.Data, c2vMsg)
	if err != nil {
		return err
	}

	req := &metadatapb.MissingK8SMetadataRequest{}
	err = types.UnmarshalAny(c2vMsg.Msg, req)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal metadata req message")
		return err
	}

	log.
		WithField("from", req.FromUpdateVersion).
		WithField("to", req.ToUpdateVersion).
		Info("Got request for missing metadata updates")

	batches, firstAvailable, lastAvailable, err := m.getUpdatesInBatches(req.FromUpdateVersion, req.ToUpdateVersion, req.Selector)
	if err != nil {
		return err
	}

	for _, batch := range batches {
		resp := &metadatapb.MissingK8SMetadataResponse{
			Updates:              batch,
			FirstUpdateAvailable: firstAvailable,
			LastUpdateAvailable:  lastAvailable,
		}

		respAnyMsg, err := types.MarshalAny(resp)
		if err != nil {
			return err
		}
		v2cMsg := cvmsgspb.V2CMessage{
			Msg: respAnyMsg,
		}
		b, err := v2cMsg.Marshal()
		if err != nil {
			return err
		}

		missingResponseTopic := fmt.Sprintf("%s:%s", metadataResponseTopic, req.CustomTopic)

		err = m.sendMessage(messagebus.V2CTopic(missingResponseTopic), b)
		if err != nil {
			return err
		}
	}

	return nil
}
