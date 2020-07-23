package controllers

import (
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
)

// TopicListener handles NATS messages for a specific topic.
type TopicListener interface {
	HandleMessage(*nats.Msg) error
}

// SendMessageFn is the function the TopicListener uses to publish messages back to NATS.
type SendMessageFn func(string, []byte) error

// MessageBusController handles and responds to any incoming NATS messages.
type MessageBusController struct {
	conn *nats.Conn
	ch   chan *nats.Msg

	isLeader      *bool
	listeners     map[string]TopicListener // Map from topic to its listener.
	subscriptions []*nats.Subscription
}

// NewMessageBusController creates a new controller for handling NATS messages.
func NewMessageBusController(conn *nats.Conn, agentTopic string, agentManager AgentManager, probeManager *ProbeManager, mdStore MetadataStore, mdHandler *MetadataHandler, isLeader *bool) (*MessageBusController, error) {
	ch := make(chan *nats.Msg, 8192)
	listeners := make(map[string]TopicListener)
	subscriptions := make([]*nats.Subscription, 0)
	mc := &MessageBusController{conn: conn, isLeader: isLeader, ch: ch, listeners: listeners, subscriptions: subscriptions}

	mc.registerListeners(agentTopic, agentManager, probeManager, mdStore, mdHandler)

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

func (mc *MessageBusController) registerListeners(agentTopic string, agentManager AgentManager, probeManager *ProbeManager, mdStore MetadataStore, mdHandler *MetadataHandler) error {
	// Register AgentTopicListener.
	atl, err := NewAgentTopicListener(agentManager, probeManager, mdStore, mc.sendMessage)
	if err != nil {
		return err
	}
	err = mc.registerListener(agentTopic, atl)
	if err != nil {
		return err
	}

	// Register MetadataTopicListener.
	ml, err := NewMetadataTopicListener(mdStore, mdHandler, mc.sendMessage)
	if err != nil {
		return err
	}
	err = mc.registerListener(MetadataRequestSubscribeTopic, ml)
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
		sub.Unsubscribe()
		sub.Drain()
	}

	mc.conn.Drain()
	mc.conn.Close()
}
