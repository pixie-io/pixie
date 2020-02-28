package controllers

import (
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	messages "pixielabs.ai/pixielabs/src/shared/messages/messagespb"
)

// MetadataRequestSubscribeTopic is the channel which the listener is subscribed to for metadata requests.
const MetadataRequestSubscribeTopic = "c2v.MetadataRequest"

// MetadataRequestPublishTopic is the channel which the listener publishes metadata responses to.
const MetadataRequestPublishTopic = "v2c.MetadataResponse"

// MetadataUpdatesTopic is the channel which the listener publishes metadata updates to.
const MetadataUpdatesTopic = "v2c.DurableMetadataUpdates"

// MetadataTopicListener is responsible for listening to and handling messages on the metadata update topic.
type MetadataTopicListener struct {
	sendMessage SendMessageFn
	mds         MetadataStore
	mh          *MetadataHandler
}

// NewMetadataTopicListener creates a new metadata topic listener.
func NewMetadataTopicListener(mdStore MetadataStore, mdHandler *MetadataHandler, sendMsgFn SendMessageFn) (*MetadataTopicListener, error) {
	m := &MetadataTopicListener{
		sendMessage: sendMsgFn,
		mds:         mdStore,
		mh:          mdHandler,
	}

	// Subscribe to metadata updates.
	mdHandler.AddSubscriber(m)

	return m, nil
}

// HandleMessage handles a message on the agent topic.
func (m *MetadataTopicListener) HandleMessage(msg *nats.Msg) error {
	// TODO(michelle): This should respond to MetadataUpdateRequests.

	return nil
}

// HandleUpdate sends the metadata update over the message bus.
func (m *MetadataTopicListener) HandleUpdate(update *UpdateMessage) {
	if update.NodeSpecific { // The metadata update is an update for a specific agent.
		return
	}

	msg := messages.MetadataUpdate{
		Update: update.Message,
	}
	b, err := msg.Marshal()
	if err != nil {
		log.WithError(err).Error("Could not marshal update message")
		return
	}

	err = m.sendMessage(MetadataUpdatesTopic, b)
	if err != nil {
		log.WithError(err).Error("Could not send metadata update over NATS")
	}
}
