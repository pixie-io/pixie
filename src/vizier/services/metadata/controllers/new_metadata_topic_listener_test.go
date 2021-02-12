package controllers

import (
	"math"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

type FakeStore struct{}

func (s *FakeStore) FetchResourceUpdates(topic string, from int64, to int64) ([]*storepb.K8SResourceUpdate, error) {
	updates := make([]*storepb.K8SResourceUpdate, 0)
	i := from
	for i < to {
		updates = append(updates, &storepb.K8SResourceUpdate{
			Update: &metadatapb.ResourceUpdate{
				UpdateVersion: i,
			},
		})
		i++
	}

	if topic == "127.0.0.1" {
		return updates, nil
	}

	return nil, nil
}

func (s *FakeStore) AddResourceUpdateForTopic(uv int64, topic string, r *storepb.K8SResourceUpdate) error {
	return nil
}

func (s *FakeStore) AddResourceUpdate(uv int64, r *storepb.K8SResourceUpdate) error {
	return nil
}

func (s *FakeStore) AddFullResourceUpdate(uv int64, r *storepb.K8SResource) error {
	return nil
}

func (s *FakeStore) GetUpdateVersion(topic string) (int64, error) {
	return 0, nil
}

func (s *FakeStore) SetUpdateVersion(topic string, uv int64) error {
	return nil
}

func (s *FakeStore) FetchFullResourceUpdates(from int64, to int64) ([]*storepb.K8SResource, error) {
	return nil, nil
}

func TestMetadataTopicListener_GetUpdatesInBatches(t *testing.T) {
	tests := []struct {
		name               string
		firstAvailable     int64
		lastAvailable      int64
		expectedNumBatches int
	}{
		{
			name:               "single update",
			firstAvailable:     1,
			lastAvailable:      1,
			expectedNumBatches: 1,
		},
		{
			name:               "batch sized update",
			firstAvailable:     3,
			lastAvailable:      26,
			expectedNumBatches: 1,
		},
		{
			name:               "greater than one batch update",
			firstAvailable:     3,
			lastAvailable:      27,
			expectedNumBatches: 2,
		},
		{
			name:               "multiple batches",
			firstAvailable:     102,
			lastAvailable:      181,
			expectedNumBatches: 4,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			mds := &FakeStore{}
			updateCh := make(chan *K8sResourceMessage)
			mdh := NewK8sMetadataHandler(updateCh, mds, nil)
			mdTL, err := NewMetadataTopicListener(nil, nil, mdh, func(topic string, b []byte) error {
				return nil
			})

			batches, firstAvailable, lastAvailable, err := mdTL.getUpdatesInBatches(test.firstAvailable, test.lastAvailable+1, "127.0.0.1")
			assert.Nil(t, err)
			assert.Equal(t, test.expectedNumBatches, len(batches))
			assert.Equal(t, test.firstAvailable, firstAvailable)
			assert.Equal(t, test.lastAvailable, lastAvailable)

			// Check batches contain all updates.
			currVersion := test.firstAvailable
			currBatch := 0
			for currBatch < len(batches) {
				if currBatch == len(batches)-1 {
					lastBatchSize := int(math.Mod(float64(test.lastAvailable-test.firstAvailable+1), batchSize))
					if lastBatchSize == 0 { // (batchSize % batchSize = 0)
						lastBatchSize = batchSize
					}
					assert.Equal(t, lastBatchSize, len(batches[currBatch]))
				} else {
					assert.Equal(t, batchSize, len(batches[currBatch]))
				}

				for _, b := range batches[currBatch] {
					assert.Equal(t, currVersion, b.UpdateVersion)
					currVersion++
				}

				currBatch++
			}
		})
	}
}

func TestMetadataTopicListener_ProcessAgentMessage(t *testing.T) {
	mds := &FakeStore{}
	updateCh := make(chan *K8sResourceMessage)
	mdh := NewK8sMetadataHandler(updateCh, mds, nil)

	sentUpdates := make([]*messages.VizierMessage, 0)
	mdTL, err := NewMetadataTopicListener(nil, nil, mdh, func(topic string, b []byte) error {
		vzMsg := &messages.VizierMessage{}
		err := proto.Unmarshal(b, vzMsg)
		if err != nil {
			return err
		}
		sentUpdates = append(sentUpdates, vzMsg)
		return nil
	})

	req := messages.VizierMessage{
		Msg: &messages.VizierMessage_K8SMetadataMessage{
			K8SMetadataMessage: &messages.K8SMetadataMessage{
				Msg: &messages.K8SMetadataMessage_MissingK8SMetadataRequest{
					MissingK8SMetadataRequest: &metadatapb.MissingK8SMetadataRequest{
						IP:                "127.0.0.1",
						FromUpdateVersion: 102,
						ToUpdateVersion:   182,
					},
				},
			},
		},
	}
	b, err := req.Marshal()
	assert.Nil(t, err)

	natsMsg := nats.Msg{}
	natsMsg.Data = b
	err = mdTL.processAgentMessage(&natsMsg)
	assert.Nil(t, err)

	assert.Equal(t, 4, len(sentUpdates))
	// Check first/last available set on all sent batches, and that updates are included.
	for _, u := range sentUpdates {
		resp := u.GetK8SMetadataMessage().GetMissingK8SMetadataResponse()
		assert.Equal(t, int64(102), resp.FirstUpdateAvailable)
		assert.Equal(t, int64(181), resp.LastUpdateAvailable)
		assert.NotEqual(t, 0, len(resp.Updates))
	}
}
