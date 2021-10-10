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

package controllers

import (
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/vizier/services/metadata/controllers/agent"
	"px.dev/pixie/src/vizier/services/metadata/controllers/k8smeta"
	"px.dev/pixie/src/vizier/services/metadata/controllers/tracepoint"
)

const updateAgentTopic = "UpdateAgent"

// TopicListener handles NATS messages for a specific topic.
type TopicListener interface {
	Initialize() error
	HandleMessage(*nats.Msg) error
	Stop()
}

// SendMessageFn is the function the TopicListener uses to publish messages back to NATS.
type SendMessageFn func(string, []byte) error

// MessageBusController handles and responds to any incoming NATS messages.
type MessageBusController struct {
	conn *nats.Conn
	ch   chan *nats.Msg

	wasLeader     bool
	isLeader      *bool
	listeners     map[string]TopicListener // Map from topic to its listener.
	subscriptions []*nats.Subscription
}

// NewMessageBusController creates a new controller for handling NATS messages.
func NewMessageBusController(conn *nats.Conn, agtMgr agent.Manager,
	tpMgr *tracepoint.Manager, k8smetaHandler *k8smeta.Handler,
	isLeader *bool) (*MessageBusController, error) {
	ch := make(chan *nats.Msg, 8192)
	listeners := make(map[string]TopicListener)
	subscriptions := make([]*nats.Subscription, 0)
	mc := &MessageBusController{
		conn:          conn,
		wasLeader:     *isLeader,
		isLeader:      isLeader,
		ch:            ch,
		listeners:     listeners,
		subscriptions: subscriptions,
	}

	err := mc.registerListeners(agtMgr, tpMgr, k8smetaHandler)
	if err != nil {
		return nil, err
	}

	// Start listening to messages.
	go mc.handleMessages()

	return mc, nil
}

func (mc *MessageBusController) handleMessages() {
	for {
		msg, more := <-mc.ch
		if !more {
			return
		}

		if !mc.wasLeader && *mc.isLeader {
			// Gained leadership!
			err := mc.listeners[updateAgentTopic].Initialize()
			if err != nil {
				log.WithError(err).Error("Failed to initialize update agent topic")
			}
		}

		mc.wasLeader = *mc.isLeader

		if !*mc.isLeader {
			continue
		}

		if tl, ok := mc.listeners[msg.Subject]; ok {
			err := tl.HandleMessage(msg)
			if err != nil {
				log.WithError(err).Error("Error handling NATs message")
			}
			continue
		}
		log.WithField("topic", msg.Subject).Info("No registered message bus listener")
	}
}

func (mc *MessageBusController) registerListeners(agtMgr agent.Manager, tpMgr *tracepoint.Manager, k8smetaHandler *k8smeta.Handler) error {
	// Register AgentTopicListener.
	atl, err := NewAgentTopicListener(agtMgr, tpMgr, mc.sendMessage)
	if err != nil {
		return err
	}
	err = mc.registerListener(updateAgentTopic, atl)
	if err != nil {
		return err
	}

	// Register MetadataTopicListener.
	ml, err := k8smeta.NewMetadataTopicListener(k8smetaHandler, mc.sendMessage)
	if err != nil {
		return err
	}
	err = mc.registerListener(k8smeta.MetadataRequestSubscribeTopic, ml)
	if err != nil {
		return err
	}
	err = mc.registerListener(k8smeta.MissingMetadataRequestTopic, ml)
	if err != nil {
		return err
	}

	return nil
}

func (mc *MessageBusController) sendMessage(topic string, msg []byte) error {
	err := mc.conn.Publish(topic, msg)
	if err != nil {
		log.WithError(err).Error("Could not publish message to message bus.")
		return err
	}
	return nil
}

func (mc *MessageBusController) registerListener(topic string, tl TopicListener) error {
	mc.listeners[topic] = tl

	sub, err := mc.conn.ChanSubscribe(topic, mc.ch)
	if err != nil {
		return err
	}
	mc.subscriptions = append(mc.subscriptions, sub)
	return nil
}

// Close closes the subscription and NATS connection.
func (mc *MessageBusController) Close() {
	for _, sub := range mc.subscriptions {
		err := sub.Unsubscribe()
		if err != nil {
			log.WithError(err).Warn("Failed to unsubscribe")
		}
		err = sub.Drain()
		if err != nil {
			log.WithError(err).Warn("Failed to drain subscription")
		}
	}

	err := mc.conn.Drain()
	if err != nil {
		log.WithError(err).Warn("Failed to drain nats connection")
	}
	mc.conn.Close()

	for _, tl := range mc.listeners {
		tl.Stop()
	}
}
