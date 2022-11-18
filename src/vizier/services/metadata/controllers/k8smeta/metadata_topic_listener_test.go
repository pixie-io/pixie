/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package k8smeta

import (
	"math"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
)

const mdBatchSize = 128

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

func (s *FakeStore) SetPodLabels(namespace string, podName string, labels map[string]string) error {
	return nil
}

func (s *FakeStore) DeletePodLabels(namespace string, podName string) error {
	return nil
}

func (s *FakeStore) FetchPodsWithLabelKey(namespace string, key string) ([]string, error) {
	return nil, nil
}

func (s *FakeStore) FetchPodsWithLabels(namespace string, labels map[string]string) ([]string, error) {
	return nil, nil
}

func (s *FakeStore) GetWithPrefix(prefix string) ([]string, [][]byte, error) {
	return nil, nil, nil
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
			lastAvailable:      3 + mdBatchSize - 1,
			expectedNumBatches: 1,
		},
		{
			name:               "greater than one batch update",
			firstAvailable:     3,
			lastAvailable:      3 + mdBatchSize,
			expectedNumBatches: 2,
		},
		{
			name:               "multiple batches",
			firstAvailable:     102,
			lastAvailable:      102 + 4*mdBatchSize - 1,
			expectedNumBatches: 4,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			mds := &FakeStore{}
			updateCh := make(chan *K8sResourceMessage)
			mdh := NewHandler(updateCh, mds, mds, nil)
			mdTL, err := NewMetadataTopicListener(mdh, func(topic string, b []byte) error {
				return nil
			})
			require.NoError(t, err)

			batches, firstAvailable, lastAvailable, err := mdTL.getUpdatesInBatches(test.firstAvailable, test.lastAvailable+1, "127.0.0.1")
			require.NoError(t, err)
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
	mdh := NewHandler(updateCh, mds, mds, nil)

	sentUpdates := make([]*messagespb.VizierMessage, 0)
	mdTL, err := NewMetadataTopicListener(mdh, func(topic string, b []byte) error {
		vzMsg := &messagespb.VizierMessage{}
		err := proto.Unmarshal(b, vzMsg)
		if err != nil {
			return err
		}
		sentUpdates = append(sentUpdates, vzMsg)
		return nil
	})
	require.NoError(t, err)

	req := messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_K8SMetadataMessage{
			K8SMetadataMessage: &messagespb.K8SMetadataMessage{
				Msg: &messagespb.K8SMetadataMessage_MissingK8SMetadataRequest{
					MissingK8SMetadataRequest: &metadatapb.MissingK8SMetadataRequest{
						Selector:          "127.0.0.1",
						FromUpdateVersion: 102,
						ToUpdateVersion:   102 + 4*mdBatchSize - 1,
					},
				},
			},
		},
	}
	b, err := req.Marshal()
	require.NoError(t, err)

	natsMsg := nats.Msg{}
	natsMsg.Data = b
	err = mdTL.processAgentMessage(&natsMsg)
	require.NoError(t, err)

	assert.Equal(t, 4, len(sentUpdates))
	// Check first/last available set on all sent batches, and that updates are included.
	for _, u := range sentUpdates {
		resp := u.GetK8SMetadataMessage().GetMissingK8SMetadataResponse()
		assert.Equal(t, int64(102), resp.FirstUpdateAvailable)
		assert.Equal(t, int64(102+4*mdBatchSize-2), resp.LastUpdateAvailable)
		assert.NotEqual(t, 0, len(resp.Updates))
	}
}
