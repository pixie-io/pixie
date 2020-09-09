package controllers

import (
	"math"

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
	// MetadataUpdatesTopic is the channel which the listener publishes metadata updates to.
	MetadataUpdatesTopic = messagebus.V2CTopic("DurableMetadataUpdates")
)

const subscriberName = "cloud"
const batchSize = 24

// MetadataTopicListener is responsible for listening to and handling messages on the metadata update topic.
type MetadataTopicListener struct {
	sendMessage SendMessageFn
	mds         MetadataStore
	mh          *MetadataHandler
	msgCh       chan *nats.Msg
	quitCh      chan bool
}

// NewMetadataTopicListener creates a new metadata topic listener.
func NewMetadataTopicListener(mdStore MetadataStore, mdHandler *MetadataHandler, sendMsgFn SendMessageFn) (*MetadataTopicListener, error) {
	m := &MetadataTopicListener{
		sendMessage: sendMsgFn,
		mds:         mdStore,
		mh:          mdHandler,
		msgCh:       make(chan *nats.Msg, 1000),
		quitCh:      make(chan bool),
	}

	m.mds.UpdateSubscriberResourceVersion(subscriberName, "")

	// Subscribe to metadata updates.
	mdHandler.AddSubscriber(m)

	go m.processMessages()

	return m, nil
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

// Stop stops processing any metadata messages.
func (m *MetadataTopicListener) Stop() {
	m.quitCh <- true
}

// ProcessMessage processes a single message in the metadata topic.
func (m *MetadataTopicListener) ProcessMessage(msg *nats.Msg) error {
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

	updates, err := m.mds.GetMetadataUpdatesForHostname(nil, pb.From, pb.To)
	if err != nil {
		return err
	}

	// Send updates in batches.
	batch := 0
	for batch*batchSize < len(updates) {
		batchSlice := updates[batch*batchSize : int(math.Min(float64((batch+1)*batchSize), float64(len(updates))))]

		resp := cvmsgspb.MetadataResponse{
			Updates: batchSlice,
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
		err = m.sendMessage(messagebus.V2CTopic(pb.Topic), b)
		if err != nil {
			return err
		}
		batch++
	}

	return nil
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
		updates, err := m.mds.GetMetadataUpdatesForHostname(nil, "", update.Message.ResourceVersion)
		if err != nil {
			log.WithError(err).Error("Could not fetch previous updates to get PrevResourceVersion")
			return
		}
		if len(updates) > 0 {
			prevRV = updates[len(updates)-1].ResourceVersion
		}
	}

	// Don't send updates we have already sent before.
	if update.Message.ResourceVersion <= prevRV {
		log.WithField("update", update).Trace("Received old update that should have already been sent")
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
