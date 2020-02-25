package controllers

import (
	"sort"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/go-nats"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	messages "pixielabs.ai/pixielabs/src/shared/messages/messagespb"
)

// MetadataTopic is the channel which the listener is subscribed to.
const MetadataTopic = "metadata_updates"

//MetadataPublishTopic is the channel which the listener publishes to.
const MetadataPublishTopic = "cloud_metadata_updates" // TODO: Actually get the correct topic names.

// MetadataTopicListener is responsible for listening to and handling messages on the metadata update topic.
type MetadataTopicListener struct {
	sendMessage SendMessageFn
	mds         MetadataStore
}

// NewMetadataTopicListener creates a new metadata topic listener.
func NewMetadataTopicListener(mdStore MetadataStore, sendMsgFn SendMessageFn) (*MetadataTopicListener, error) {
	return &MetadataTopicListener{
		sendMessage: sendMsgFn,
		mds:         mdStore,
	}, nil
}

// HandleMessage handles a message on the agent topic.
func (m *MetadataTopicListener) HandleMessage(msg *nats.Msg) error {
	pb := &messages.MetadataUpdatesRequest{}
	err := proto.Unmarshal(msg.Data, pb)
	if err != nil {
		return err
	}

	// Send existing metadata.
	updates, err := m.getUpdates(pb.ResourceVersion)
	if err != nil {
		return err
	}
	for i := range updates {
		err = m.sendUpdate(updates[i])
		if err != nil {
			return err
		}
	}

	// TODO(michelle): Subscribe to future metadata updates. This will require a
	// little bit of refactoring in the metadata handler to do this cleanly. This will
	// come in the next diff.

	return nil
}

func (m *MetadataTopicListener) sendUpdate(update *metadatapb.ResourceUpdate) error {
	msg := messages.MetadataUpdate{
		Update: update,
	}
	b, err := msg.Marshal()
	if err != nil {
		return err
	}

	return m.sendMessage(MetadataPublishTopic, b)
}

func (m *MetadataTopicListener) getUpdates(resourceVersion string) ([]*metadatapb.ResourceUpdate, error) {
	updates, err := m.mds.GetMetadataUpdates("")
	if err != nil {
		return nil, err
	}

	filteredUpdates := make([]*metadatapb.ResourceUpdate, 0)
	if resourceVersion != "" {
		// Filter out any updates with a smaller resourceVersion.
		for i, u := range updates {
			if u.ResourceVersion >= resourceVersion {
				filteredUpdates = append(filteredUpdates, updates[i])
			}
		}
	} else {
		filteredUpdates = updates
	}

	// Updates should be sent in resourceVersion order.
	sort.Slice(filteredUpdates, func(i, j int) bool {
		return filteredUpdates[i].ResourceVersion < filteredUpdates[j].ResourceVersion
	})

	return filteredUpdates, nil
}
