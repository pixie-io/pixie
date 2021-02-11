package controllers

import (
	"fmt"
	"math"
	"sort"
	"strings"
	"sync"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/utils/messagebus"
)

var (
	// MetadataRequestSubscribeTopic is the channel which the listener is subscribed to for metadata requests.
	MetadataRequestSubscribeTopic = messagebus.C2VTopic("MetadataRequest")
	// MetadataUpdatesTopic is the channel which the listener publishes metadata updates to.
	MetadataUpdatesTopic = messagebus.V2CTopic("DurableMetadataUpdates")
	// MissingMetadataRequestTopic is the channel which the listener should listen to missing metadata update requests on.
	MissingMetadataRequestTopic = "MissingMetadataRequests"
)

const subscriberName = "cloud"
const batchSize = 24

// MetadataTopicListener is responsible for listening to and handling messages on the metadata update topic.
type MetadataTopicListener struct {
	sendMessage SendMessageFn
	mds         MetadataStore
	mh          *MetadataHandler
	newMh       *K8sMetadataHandler
	msgCh       chan *nats.Msg

	once   sync.Once
	quitCh chan struct{}
}

// NewMetadataTopicListener creates a new metadata topic listener.
func NewMetadataTopicListener(mdStore MetadataStore, mdHandler *MetadataHandler, newMdHandler *K8sMetadataHandler, sendMsgFn SendMessageFn) (*MetadataTopicListener, error) {
	m := &MetadataTopicListener{
		sendMessage: sendMsgFn,
		mds:         mdStore,
		mh:          mdHandler,
		newMh:       newMdHandler,
		msgCh:       make(chan *nats.Msg, 1000),
		quitCh:      make(chan struct{}),
	}

	// TODO(michelle, vihang): Remove the old metadatastore and handler.
	if mdStore != nil {
		m.mds.UpdateSubscriberResourceVersion(subscriberName, "")
	}
	if mdHandler != nil {
		// Subscribe to metadata updates.
		mdHandler.AddSubscriber(m)
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

// Stop stops processing any metadata messages.
func (m *MetadataTopicListener) Stop() {
	m.once.Do(func() {
		close(m.quitCh)
	})
}

func sortResourceUpdates(arr []*metadatapb.ResourceUpdate) {
	sortFn := func(i, j int) bool {
		return compareResourceVersions(arr[i].ResourceVersion, arr[j].ResourceVersion) < 0
	}

	sort.Slice(arr, sortFn)
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
	vzMsg := &messages.VizierMessage{}
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

	batches, firstAvailable, lastAvailable, err := m.getUpdatesInBatches(req.FromUpdateVersion, req.ToUpdateVersion, req.IP)
	if err != nil {
		return err
	}

	for _, b := range batches {
		resp := &messages.VizierMessage{
			Msg: &messages.VizierMessage_K8SMetadataMessage{
				K8SMetadataMessage: &messages.K8SMetadataMessage{
					Msg: &messages.K8SMetadataMessage_MissingK8SMetadataResponse{
						MissingK8SMetadataResponse: &metadatapb.MissingK8SMetadataResponse{
							Updates:              b,
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
		err = m.sendMessage(getK8sUpdateChannel(req.IP), b)
		if err != nil {
			return err
		}
	}

	return nil
}

func (m *MetadataTopicListener) getUpdatesInBatches(from int64, to int64, ip string) ([][]*metadatapb.ResourceUpdate, int64, int64, error) {
	updates, err := m.newMh.GetUpdatesForIP(ip, from, to)
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

// TODO(michelle,vihang): This should be updated to use the new k8s metadata handler.
func (m *MetadataTopicListener) processCloudMessage(msg *nats.Msg) error {
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

	var updates []*metadatapb.ResourceUpdate
	if pb.From == "" {
		// Get current state using the pods/endpoints/containers known in etcd.
		mdUpdates, mdErr := m.mds.GetMetadataUpdates(nil)
		if mdErr != nil {
			log.WithError(mdErr).Error("Could not get metadata updates")
			return mdErr
		}

		// Sort updates.
		sortResourceUpdates(mdUpdates)

		// Assign prevRVs and filter out any RVs greater than the To request.
		updates = make([]*metadatapb.ResourceUpdate, 0)
		currRV := ""
		for _, u := range mdUpdates {
			u.PrevResourceVersion = currRV
			currRV = u.ResourceVersion
			updates = append(updates, u)
		}
	} else {
		updates, err = m.mds.GetMetadataUpdatesForHostname(nil, pb.From, pb.To)
		if err != nil {
			return err
		}
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

func compareResourceVersions(rv1 string, rv2 string) int {
	// The rv may end in "_#", for containers which share the same rv as pods.
	// This needs to be removed from the string that is about to be padded, and reappended after padding.
	formatRV := func(rv string) string {
		splitRV := strings.Split(rv, "_")
		paddedRV := fmt.Sprintf("%020s", splitRV[0])
		if len(splitRV) > 1 {
			// Reappend the suffix, if any.
			paddedRV = fmt.Sprintf("%s_%s", paddedRV, splitRV[1])
		}
		return paddedRV
	}

	fmtRV1 := formatRV(rv1)
	fmtRV2 := formatRV(rv2)

	if fmtRV1 == fmtRV2 {
		return 0
	}
	if fmtRV1 < fmtRV2 {
		return -1
	}
	return 1
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
		// Get current state using the pods/endpoints/containers known in etcd.
		updates, err := m.mds.GetMetadataUpdates(nil)
		if err != nil {
			log.WithError(err).Error("Could not get metadata updates")
			return
		}

		// Sort updates, since they may not be returned in-order from GetMetadataUpdates.
		sortResourceUpdates(updates)

		// Get the most recent update. This is the prevRV we will use for the incoming update.
		prevRV = updates[len(updates)-1].ResourceVersion
		m.mds.UpdateSubscriberResourceVersion(subscriberName, prevRV)
	}

	// Don't send updates we have already sent before.
	if compareResourceVersions(update.Message.ResourceVersion, prevRV) < 1 {
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
