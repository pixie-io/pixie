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

package controllers_test

import (
	"errors"
	"fmt"
	"sync"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/cloud/vzmgr/controllers"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/utils/testingutils"
)

type MetadataRequest struct {
	from      int64
	to        int64
	responses []*metadatapb.MissingK8SMetadataResponse
}

func listenForUnexpectedIndexerMessages(c chan *nats.Msg) error {
	run := true
	for run {
		select {
		case idxMessage := <-c:
			u := &metadatapb.ResourceUpdate{}
			err := proto.Unmarshal(idxMessage.Data, u)
			if err != nil {
				return err
			}
			return fmt.Errorf("Unexpected index message: %d", u.UpdateVersion)
		// On some investigations, found that messages are sent < 500us, chose a
		// timeout that was 100x in case of any unexpected interruptions.
		case <-time.After(50 * time.Millisecond):
			run = false
		}
	}
	return nil
}

func TestMetadataReader_ProcessVizierUpdate(t *testing.T) {
	tests := []struct {
		name                    string                       // Name of the test
		oldMetadataIndexUpdates []int64                      // Updates that already exist on the MetadataIndex channel.
		jsMetadataUpdates       []*metadatapb.ResourceUpdate // The updates sent on JetStream
		missingMetadataCalls    []*MetadataRequest           // The expected metadata request and responses.
		expectedIndexerUpdates  []*metadatapb.ResourceUpdate // The updates that should be sent to the indexer.
		vizierStatus            string
	}{
		{
			name: "In-order update",
			jsMetadataUpdates: []*metadatapb.ResourceUpdate{
				{PrevUpdateVersion: 1234, UpdateVersion: 1235},
			},
			missingMetadataCalls: []*MetadataRequest{
				{
					from: 0,
					to:   1235,
					responses: []*metadatapb.MissingK8SMetadataResponse{
						{
							FirstUpdateAvailable: 1235,
						},
					},
				},
			},
			expectedIndexerUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 1235, PrevUpdateVersion: 1234},
			},
			vizierStatus: "HEALTHY",
		},
		{
			name: "out-of-order update",
			jsMetadataUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 17, PrevUpdateVersion: 16},
			},
			missingMetadataCalls: []*MetadataRequest{
				{
					from: 0,
					to:   17,
					responses: []*metadatapb.MissingK8SMetadataResponse{
						{
							FirstUpdateAvailable: 12,
							Updates: []*metadatapb.ResourceUpdate{
								{UpdateVersion: 12, PrevUpdateVersion: 11},
								{UpdateVersion: 13, PrevUpdateVersion: 12},
								{UpdateVersion: 15, PrevUpdateVersion: 13},
								{UpdateVersion: 16, PrevUpdateVersion: 15},
							},
						},
					},
				},
			},
			expectedIndexerUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 12, PrevUpdateVersion: 11},
				{UpdateVersion: 13, PrevUpdateVersion: 12},
				{UpdateVersion: 15, PrevUpdateVersion: 13},
				{UpdateVersion: 16, PrevUpdateVersion: 15},
				{UpdateVersion: 17, PrevUpdateVersion: 16},
			},
			vizierStatus: "UNHEALTHY",
		},
		{
			name: "multiple out-of-order update",
			jsMetadataUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 17, PrevUpdateVersion: 16},
			},
			missingMetadataCalls: []*MetadataRequest{
				{
					from: 0,
					to:   17,
					responses: []*metadatapb.MissingK8SMetadataResponse{
						{
							FirstUpdateAvailable: 12,
							Updates: []*metadatapb.ResourceUpdate{
								{UpdateVersion: 12, PrevUpdateVersion: 11},
								{UpdateVersion: 13, PrevUpdateVersion: 12},
							},
						},
						{
							FirstUpdateAvailable: 12,
							Updates: []*metadatapb.ResourceUpdate{
								{UpdateVersion: 16, PrevUpdateVersion: 15},
							},
						},
					},
				},
				{
					from: 13,
					to:   16,
					responses: []*metadatapb.MissingK8SMetadataResponse{
						{
							FirstUpdateAvailable: 12,
							Updates: []*metadatapb.ResourceUpdate{
								{UpdateVersion: 15, PrevUpdateVersion: 13},
							},
						},
					},
				},
			},
			expectedIndexerUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 12, PrevUpdateVersion: 11},
				{UpdateVersion: 13, PrevUpdateVersion: 12},
				{UpdateVersion: 15, PrevUpdateVersion: 13},
				{UpdateVersion: 16, PrevUpdateVersion: 15},
				{UpdateVersion: 17, PrevUpdateVersion: 16},
			},
			vizierStatus: "HEALTHY",
		},
		{
			name: "duplicate JetStream updates",
			jsMetadataUpdates: []*metadatapb.ResourceUpdate{
				{PrevUpdateVersion: 1234, UpdateVersion: 1235},
				{PrevUpdateVersion: 1234, UpdateVersion: 1235},
				{PrevUpdateVersion: 1234, UpdateVersion: 1235},
				{PrevUpdateVersion: 1235, UpdateVersion: 1236},
				{PrevUpdateVersion: 1235, UpdateVersion: 1236},
				{PrevUpdateVersion: 1236, UpdateVersion: 1237},
			},
			missingMetadataCalls: []*MetadataRequest{
				{
					from: 0,
					to:   1235,
					responses: []*metadatapb.MissingK8SMetadataResponse{
						{
							FirstUpdateAvailable: 1235,
						},
					},
				},
			},
			expectedIndexerUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 1235, PrevUpdateVersion: 1234},
				{UpdateVersion: 1236, PrevUpdateVersion: 1235},
				{UpdateVersion: 1237, PrevUpdateVersion: 1236},
			},
			vizierStatus: "HEALTHY",
		},
		{
			name: "disconnected vizier",
			jsMetadataUpdates: []*metadatapb.ResourceUpdate{
				{PrevUpdateVersion: 1, UpdateVersion: 2},
			},
			expectedIndexerUpdates: []*metadatapb.ResourceUpdate{},
			vizierStatus:           "DISCONNECTED",
		},
		{
			name: "duplicateUpdate",
			jsMetadataUpdates: []*metadatapb.ResourceUpdate{
				{PrevUpdateVersion: 1, UpdateVersion: 2},
			},
			expectedIndexerUpdates: []*metadatapb.ResourceUpdate{},
			vizierStatus:           "HEALTHY",
		},
		{
			name: "Metadata missing but k8s also missing the metadata on restart",
			jsMetadataUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 1003, PrevUpdateVersion: 1002},
			},
			missingMetadataCalls: []*MetadataRequest{
				{
					from: 1001,
					to:   1003,
					responses: []*metadatapb.MissingK8SMetadataResponse{
						{
							FirstUpdateAvailable: 1003,
						},
					},
				},
			},
			expectedIndexerUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 1003, PrevUpdateVersion: 1002},
			},
			oldMetadataIndexUpdates: []int64{999, 1000, 1001},
			vizierStatus:            "HEALTHY",
		},
		{
			name: "Expect Full Metadata Update requests on restart",
			jsMetadataUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 1005, PrevUpdateVersion: 1004},
			},
			missingMetadataCalls: []*MetadataRequest{
				{
					from: 1001,
					to:   1005,
					responses: []*metadatapb.MissingK8SMetadataResponse{
						{
							FirstUpdateAvailable: 1001,
							Updates: []*metadatapb.ResourceUpdate{
								{UpdateVersion: 1001, PrevUpdateVersion: 1000},
								{UpdateVersion: 1002, PrevUpdateVersion: 1001},
								{UpdateVersion: 1003, PrevUpdateVersion: 1002},
								{UpdateVersion: 1004, PrevUpdateVersion: 1003},
							},
						},
					},
				},
			},
			expectedIndexerUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 1002, PrevUpdateVersion: 1001},
				{UpdateVersion: 1003, PrevUpdateVersion: 1002},
				{UpdateVersion: 1004, PrevUpdateVersion: 1003},
				{UpdateVersion: 1005, PrevUpdateVersion: 1004},
			},
			oldMetadataIndexUpdates: []int64{999, 1000, 1001},
			vizierStatus:            "HEALTHY",
		},
		{
			name: "Expect Partial Metadata Update requests on restart",
			jsMetadataUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 1005, PrevUpdateVersion: 1004},
			},
			missingMetadataCalls: []*MetadataRequest{
				{
					from: 1001,
					to:   1005,
					responses: []*metadatapb.MissingK8SMetadataResponse{
						{
							FirstUpdateAvailable: 1003,
							Updates: []*metadatapb.ResourceUpdate{
								{UpdateVersion: 1003, PrevUpdateVersion: 1002},
								{UpdateVersion: 1004, PrevUpdateVersion: 1003},
							},
						},
					},
				},
			},
			expectedIndexerUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 1003, PrevUpdateVersion: 1002},
				{UpdateVersion: 1004, PrevUpdateVersion: 1003},
				{UpdateVersion: 1005, PrevUpdateVersion: 1004},
			},
			oldMetadataIndexUpdates: []int64{999, 1000, 1001},
			vizierStatus:            "HEALTHY",
		},
		{
			name: "Receive update already sent to indexer and do nothing on restart",
			jsMetadataUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 1000, PrevUpdateVersion: 999},
			},
			missingMetadataCalls:    []*MetadataRequest{},
			expectedIndexerUpdates:  []*metadatapb.ResourceUpdate{},
			oldMetadataIndexUpdates: []int64{999, 1000, 1001},
			vizierStatus:            "HEALTHY",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			vzID := uuid.Must(uuid.NewV4())
			orgID := uuid.Must(uuid.NewV4())

			mustLoadTestData(db)

			// Set up initial DB state.
			insertClusterQuery := `INSERT INTO vizier_cluster(id, org_id, cluster_uid) VALUES ($1, $2, 'test')`
			db.MustExec(insertClusterQuery, vzID, orgID)
			insertClusterInfoQuery := `INSERT INTO vizier_cluster_info(vizier_cluster_id, status) VALUES ($1, $2)`
			db.MustExec(insertClusterInfoQuery, vzID, test.vizierStatus)

			nc, natsCleanup := testingutils.MustStartTestNATS(t)
			defer natsCleanup()

			js := msgbus.MustConnectJetStream(nc)
			_ = js.PurgeStream("MetadataIndex")
			st, err := msgbus.NewJetStreamStreamer(nc, js, &nats.StreamConfig{
				Name: "MetadataIndex",
				Subjects: []string{
					"v2c.*.*.*",
					"MetadataIndex.*",
				},
				MaxAge: time.Minute * 2,
			})
			require.NoError(t, err)

			// First we add  entries in the indexer stream. This ensures that when we init the Vizier
			// update manager, we are peeking the bottom of the stream's queue.
			for _, updateV := range test.oldMetadataIndexUpdates {
				update := &metadatapb.ResourceUpdate{UpdateVersion: updateV}

				b, err := update.Marshal()
				require.NoError(t, err)

				// Push the update in.
				_, err = js.Publish("MetadataIndex.test", b)
				require.NoError(t, err)
			}

			idxCh := make(chan *nats.Msg)
			indexerSub, err := js.Subscribe("MetadataIndex.test", func(msg *nats.Msg) {
				u := &metadatapb.ResourceUpdate{}
				_ = proto.Unmarshal(msg.Data, u)
				idxCh <- msg
			}, nats.DeliverNew())
			if err != nil {
				t.Fatalf("failed to subscribe to JetStream: %v", err)
			}
			defer func() {
				err = indexerSub.Unsubscribe()
				require.NoError(t, err)
			}()

			batch := 0
			if test.missingMetadataCalls != nil {
				mdSub, err := nc.Subscribe(vzshard.C2VTopic("MetadataRequest", vzID), func(msg *nats.Msg) {
					c2vMsg := &cvmsgspb.C2VMessage{}
					err := proto.Unmarshal(msg.Data, c2vMsg)
					require.NoError(t, err)
					req := &metadatapb.MissingK8SMetadataRequest{}
					err = types.UnmarshalAny(c2vMsg.Msg, req)
					require.NoError(t, err)
					if len(test.missingMetadataCalls) <= batch {
						assert.FailNow(t, "unexpected missingmetadatarequest call", req)
					}
					assert.Equal(t, test.missingMetadataCalls[batch].to, req.ToUpdateVersion)
					assert.Equal(t, test.missingMetadataCalls[batch].from, req.FromUpdateVersion)

					responseTopic := fmt.Sprintf("%s:%s", "MetadataResponse", req.CustomTopic)

					// Send response.
					for _, r := range test.missingMetadataCalls[batch].responses {
						anyUpdates, err := types.MarshalAny(r)
						require.NoError(t, err)
						v2cMsg := cvmsgspb.V2CMessage{
							Msg: anyUpdates,
						}
						b, err := v2cMsg.Marshal()
						require.NoError(t, err)
						err = nc.Publish(vzshard.V2CTopic(responseTopic, vzID), b)
						require.NoError(t, err)
					}

					batch++
				})
				require.NoError(t, err)
				defer func() {
					err = mdSub.Unsubscribe()
					require.NoError(t, err)
				}()
			}

			var wg sync.WaitGroup
			wg.Add(1)
			go func() {
				defer wg.Done()
				for _, update := range test.jsMetadataUpdates {
					// Publish update to JetStream channel.
					anyInitUpdate, err := types.MarshalAny(update)
					require.NoError(t, err)
					v2cMsg := cvmsgspb.V2CMessage{
						Msg: anyInitUpdate,
					}
					b, err := v2cMsg.Marshal()
					require.NoError(t, err)

					err = st.Publish(vzshard.V2CTopic("DurableMetadataUpdates", vzID), b)
					require.NoError(t, err)
				}
			}()

			mdr, err := controllers.NewMetadataReader(db, st, nc)
			require.NoError(t, err)

			numUpdates := 0
			for numUpdates < len(test.expectedIndexerUpdates) {
				select {
				case idxMessage := <-idxCh:
					u := &metadatapb.ResourceUpdate{}
					err := proto.Unmarshal(idxMessage.Data, u)
					require.NoError(t, err)
					assert.Equal(t, test.expectedIndexerUpdates[numUpdates].UpdateVersion, u.UpdateVersion)
					numUpdates++
				case <-time.After(2 * time.Second):
					t.Fatal("Timed out")
				}
			}

			require.NoError(t, listenForUnexpectedIndexerMessages(idxCh))

			wg.Wait()
			mdr.Stop()

			// On restart, we shouldn't receive any updates.
			mdr, err = controllers.NewMetadataReader(db, st, nc)
			require.NoError(t, err)
			defer mdr.Stop()

			require.NoError(t, listenForUnexpectedIndexerMessages(idxCh))
		})
	}
}

func TestListenForUnexpectedIndexerMessages(t *testing.T) {
	nc, natsCleanup := testingutils.MustStartTestNATS(t)
	defer natsCleanup()

	js := msgbus.MustConnectJetStream(nc)
	_, err := js.StreamInfo("MetadataIndex")
	if errors.Is(err, nats.ErrStreamNotFound) {
		// Create stream if it doesn't exist.
		_, err = js.AddStream(&nats.StreamConfig{
			Name: "MetadataIndex",
			Subjects: []string{
				"v2c.*.*.*",
				"MetadataIndex.*",
			},
			MaxAge: time.Minute * 2,
		})
		require.NoError(t, err)
	} else {
		require.NoError(t, err)
	}

	idxCh := make(chan *nats.Msg)
	indexerSub, err := js.Subscribe("MetadataIndex.test", func(msg *nats.Msg) {
		idxCh <- msg
	})
	if err != nil {
		t.Fatalf("failed to subscribe to JetStream: %v", err)
	}
	defer func() {
		require.NoError(t, indexerSub.Unsubscribe())
	}()

	update := &metadatapb.ResourceUpdate{UpdateVersion: 1}
	b, err := update.Marshal()
	require.NoError(t, err)

	// Push the update in.
	_, err = js.Publish("MetadataIndex.test", b)
	require.NoError(t, err)

	// We expect this to error out because we intentionally send a message over JetStream.
	require.Error(t, listenForUnexpectedIndexerMessages(idxCh))
}
