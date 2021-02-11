package controllers

import (
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
)

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

	statsHandler *StatsHandler
}

// NewMessageBusController creates a new controller for handling NATS messages.
func NewMessageBusController(conn *nats.Conn, agentManager AgentManager,
	tracepointManager *TracepointManager, mdStore MetadataStore, newMdHandler *K8sMetadataHandler,
	statsHandler *StatsHandler, isLeader *bool) (*MessageBusController, error) {
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
		statsHandler:  statsHandler,
	}

	mc.registerListeners(agentManager, tracepointManager, mdStore, newMdHandler)

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
			mc.listeners[AgentTopic].Initialize()
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

func (mc *MessageBusController) registerListeners(agentManager AgentManager, tracepointManager *TracepointManager, mdStore MetadataStore, newMdHandler *K8sMetadataHandler) error {
	// Register AgentTopicListener.
	atl, err := NewAgentTopicListener(agentManager, tracepointManager, mdStore, mc.sendMessage, mc.statsHandler)
	if err != nil {
		return err
	}
	err = mc.registerListener(AgentTopic, atl)
	if err != nil {
		return err
	}

	// Register MetadataTopicListener.
	ml, err := NewMetadataTopicListener(newMdHandler, mc.sendMessage)
	if err != nil {
		return err
	}
	err = mc.registerListener(MetadataRequestSubscribeTopic, ml)
	if err != nil {
		return err
	}
	err = mc.registerListener(MissingMetadataRequestTopic, ml)
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

	for _, tl := range mc.listeners {
		tl.Stop()
	}
}
