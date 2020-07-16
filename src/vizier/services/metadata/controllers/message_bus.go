package controllers

import (
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
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
func NewMessageBusController(natsURL string, agentTopic string, agentManager AgentManager, mdStore MetadataStore, mdHandler *MetadataHandler, isLeader *bool) (*MessageBusController, error) {
	var conn *nats.Conn
	var err error
	if viper.GetBool("disable_ssl") {
		conn, err = nats.Connect(natsURL)
	} else {
		conn, err = nats.Connect(natsURL,
			nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
			nats.RootCAs(viper.GetString("tls_ca_cert")))
	}

	if err != nil {
		return nil, err
	}

	conn.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		log.WithError(err).
			WithField("sub", subscription.Subject).
			Error("Got nats error")
	})

	ch := make(chan *nats.Msg, 8192)
	listeners := make(map[string]TopicListener)
	subscriptions := make([]*nats.Subscription, 0)
	mc := &MessageBusController{conn: conn, isLeader: isLeader, ch: ch, listeners: listeners, subscriptions: subscriptions}

	mc.registerListeners(agentTopic, agentManager, mdStore, mdHandler)

	// Start listening to messages.
	go mc.handleMessages()

	return mc, err
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

func (mc *MessageBusController) registerListeners(agentTopic string, agentManager AgentManager, mdStore MetadataStore, mdHandler *MetadataHandler) error {
	// Register AgentTopicListener.
	atl, err := NewAgentTopicListener(agentManager, mdStore, mc.sendMessage)
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
