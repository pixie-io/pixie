package controllers

import (
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/vizier/utils/messagebus"
)

var (
	// MetadataRequestSubscribeTopic is the channel which the listener is subscribed to for metadata requests.
	MetadataRequestSubscribeTopic = messagebus.C2VTopic("MetadataRequest")
	// MetadataRequestPublishTopic is the channel which the listener publishes metadata responses to.
	MetadataRequestPublishTopic = messagebus.V2CTopic("MetadataResponse")
	// MetadataUpdatesTopic is the channel which the listener publishes metadata updates to.
	MetadataUpdatesTopic = messagebus.V2CTopic("DurableMetadataUpdates")
)

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

	m.mds.UpdateSubscriberResourceVersion(subscriberName, "")

	// Subscribe to metadata updates.
	mdHandler.AddSubscriber(m)

	return m, nil
}

// HandleMessage handles a message on the agent topic.
func (m *MetadataTopicListener) HandleMessage(msg *nats.Msg) error {
	c2vMsg := &cvmsgspb.C2VMessage{}
	err := proto.Unmarshal(msg.Data, c2vMsg)
	if err != nil {
		return err
	}

	pb := &cvmsgspb.MetadataRequest{}
	err = types.UnmarshalAny(c2vMsg.Msg, pb)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal metadata req message")
		return err
	}

	updates, err := m.mds.GetMetadataUpdatesForHostname("", pb.From, pb.To)
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

	if prevRV == "" { // This should only happen on the first update we ever send from Vizier->Cloud.
		updates, err := m.mds.GetMetadataUpdatesForHostname("", "", update.Message.ResourceVersion)
		if err != nil {
			log.WithError(err).Error("Could not fetch previous updates to get PrevResourceVersion")
			return
		}
		if len(updates) > 0 {
			prevRV = updates[len(updates)-1].ResourceVersion
		}
	}

	// We don't want to modify the prevRV in the update message for any other subscribers.
	copiedUpdate := *(update.Message)
	copiedUpdate.PrevResourceVersion = prevRV
	// Update the resource version.
	m.mds.UpdateSubscriberResourceVersion(subscriberName, copiedUpdate.ResourceVersion)

	msg := cvmsgspb.MetadataUpdate{
		Update: &copiedUpdate,
	}
	reqAnyMsg, err := types.MarshalAny(&msg)
	if err != nil {
		return
	}

	v2cMsg := cvmsgspb.V2CMessage{
		Msg: reqAnyMsg,
	}
	b, err := v2cMsg.Marshal()
	if err != nil {
		return
	}

	err = m.sendMessage(MetadataUpdatesTopic, b)
	if err != nil {
		log.WithError(err).Error("Could not send metadata update over NATS")
	}
}
