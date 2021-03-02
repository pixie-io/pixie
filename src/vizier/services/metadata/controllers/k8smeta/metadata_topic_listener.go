package k8smeta

import (
	"math"
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
	// MetadataResponseTopic is the channel which the listener uses to responsd to metadata requests.
	MetadataResponseTopic = messagebus.V2CTopic("MetadataResponse")
	// MissingMetadataRequestTopic is the channel which the listener should listen to missing metadata update requests on.
	MissingMetadataRequestTopic = "MissingMetadataRequests"
)

const subscriberName = "cloud"
const batchSize = 24

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

// Stop stops processing any metadata messages.
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

	batches, firstAvailable, lastAvailable, err := m.getUpdatesInBatches(req.FromUpdateVersion, req.ToUpdateVersion, req.Selector)
	if err != nil {
		return err
	}

	for _, batch := range batches {
		resp := &messages.VizierMessage{
			Msg: &messages.VizierMessage_K8SMetadataMessage{
				K8SMetadataMessage: &messages.K8SMetadataMessage{
					Msg: &messages.K8SMetadataMessage_MissingK8SMetadataResponse{
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

		err = m.sendMessage(MetadataResponseTopic, b)
		if err != nil {
			return err
		}
	}

	return nil
}
