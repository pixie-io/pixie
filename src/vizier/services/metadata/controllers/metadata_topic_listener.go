package controllers

import (
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
)

// MetadataRequestSubscribeTopic is the channel which the listener is subscribed to for metadata requests.
const MetadataRequestSubscribeTopic = "c2v.MetadataRequest"

// MetadataRequestPublishTopic is the channel which the listener publishes metadata responses to.
const MetadataRequestPublishTopic = "v2c.MetadataResponse"

// MetadataUpdatesTopic is the channel which the listener publishes metadata updates to.
const MetadataUpdatesTopic = "v2c.DurableMetadataUpdates"

const subscriberName = "cloud"

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
	pb := &cvmsgspb.MetadataRequest{}
	err := proto.Unmarshal(msg.Data, pb)
	if err != nil {
		return err
	}

	updates, err := m.mds.GetMetadataUpdatesForSubscriber(subscriberName, pb.From, pb.To)
	if err != nil {
		return err
	}

	resp := cvmsgspb.MetadataResponse{
		Updates: updates,
	}
	reqAnyMsg, err := types.MarshalAny(&resp)
	if err != nil {
		return err
	}

	v2cMsg := cvmsgspb.V2CMessage{
		Msg: reqAnyMsg,
	}
	b, err := v2cMsg.Marshal()
	if err != nil {
		return err
	}

	return m.sendMessage(MetadataRequestPublishTopic, b)
}

// HandleUpdate sends the metadata update over the message bus.
func (m *MetadataTopicListener) HandleUpdate(update *UpdateMessage) {
	if update.NodeSpecific { // The metadata update is an update for a specific agent.
		return
	}

	// Set previous RV on update.
	prevRV, err := m.mds.GetSubscriberResourceVersion(subscriberName)
	if err != nil {
		log.WithError(err).Error("Could not get previous resource version")
		return
	}

	// We don't want to modify the prevRV in the update message for any other subscribers.
	copiedUpdate := *(update.Message)
	copiedUpdate.PrevResourceVersion = prevRV
	// Update the resource version.
	m.mds.UpdateSubscriberResourceVersion(subscriberName, copiedUpdate.ResourceVersion)

	msg := cvmsgspb.MetadataUpdate{
		Update: &copiedUpdate,
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
