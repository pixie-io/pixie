package controller_test

import (
	"fmt"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/nats-io/stan.go"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/controller"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

type MetadataRequest struct {
	from      int64
	to        int64
	responses []*metadatapb.MissingK8SMetadataResponse
}

func TestMetadataReader_ProcessVizierUpdate(t *testing.T) {
	tests := []struct {
		name                    string                       // Name of the test
		updatePrevRV            int64                        // The prevResourceVersion of the update the metadata reader will receive.
		updateRV                int64                        // The resourceVersion of the update the metadata reader will receive.
		expectMetadataRequest   bool                         // Whether to expect a request for missing metadata.
		metadataRequests        []*MetadataRequest           // The expected metadata request and responses.
		expectedMetadataUpdates []*metadatapb.ResourceUpdate // The updates that should be sent to the indexer.
		vizierStatus            string
	}{
		{
			name:                  "In-order update",
			updatePrevRV:          1234,
			updateRV:              1235,
			expectMetadataRequest: true,
			metadataRequests: []*MetadataRequest{
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
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 1235, PrevUpdateVersion: 1234},
			},
			vizierStatus: "HEALTHY",
		},
		{
			name:                  "out-of-order update",
			updatePrevRV:          16,
			updateRV:              17,
			expectMetadataRequest: true,
			metadataRequests: []*MetadataRequest{
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
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 13, PrevUpdateVersion: 12},
				{UpdateVersion: 15, PrevUpdateVersion: 13},
				{UpdateVersion: 16, PrevUpdateVersion: 15},
				{UpdateVersion: 17, PrevUpdateVersion: 16},
			},
			vizierStatus: "UNHEALTHY",
		},
		{
			name:                  "multiple out-of-order update",
			updatePrevRV:          16,
			updateRV:              17,
			expectMetadataRequest: true,
			metadataRequests: []*MetadataRequest{
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
								{UpdateVersion: 16, PrevUpdateVersion: 15},
							},
						},
					},
				},
			},
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{
				{UpdateVersion: 13, PrevUpdateVersion: 12},
				{UpdateVersion: 15, PrevUpdateVersion: 13},
				{UpdateVersion: 16, PrevUpdateVersion: 15},
				{UpdateVersion: 17, PrevUpdateVersion: 16},
			},
			vizierStatus: "HEALTHY",
		},
		{
			name:                    "disconnected vizier",
			updatePrevRV:            1,
			updateRV:                2,
			expectMetadataRequest:   false,
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{},
			vizierStatus:            "DISCONNECTED",
		},
		{
			name:                    "duplicateUpdate",
			updatePrevRV:            1,
			updateRV:                2,
			expectMetadataRequest:   false,
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{},
			vizierStatus:            "HEALTHY",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			vzID := uuid.NewV4()
			orgID := uuid.NewV4()

			db, teardown := setupTestDB(t)
			defer teardown()

			// Set up initial DB state.
			insertClusterQuery := `INSERT INTO vizier_cluster(id, org_id, cluster_uid) VALUES ($1, $2, 'test')`
			db.MustExec(insertClusterQuery, vzID, orgID)
			insertClusterInfoQuery := `INSERT INTO vizier_cluster_info(vizier_cluster_id, status) VALUES ($1, $2)`
			db.MustExec(insertClusterInfoQuery, vzID, test.vizierStatus)

			natsPort, natsCleanup := testingutils.StartNATS(t)
			nc, err := nats.Connect(testingutils.GetNATSURL(natsPort))
			if err != nil {
				t.Fatal(err)
			}
			defer natsCleanup()

			_, sc, stanCleanup := testingutils.StartStan(t, "test-stan", "test-client")
			defer stanCleanup()

			idxCh := make(chan *stan.Msg)
			indexerSub, err := sc.Subscribe("MetadataIndex.test", func(msg *stan.Msg) {
				idxCh <- msg
			})
			defer indexerSub.Unsubscribe()

			batch := 0
			if test.expectMetadataRequest {
				mdSub, err := nc.Subscribe(vzshard.C2VTopic("MetadataRequest", vzID), func(msg *nats.Msg) {
					c2vMsg := &cvmsgspb.C2VMessage{}
					err := proto.Unmarshal(msg.Data, c2vMsg)
					assert.Nil(t, err)
					req := &metadatapb.MissingK8SMetadataRequest{}
					err = types.UnmarshalAny(c2vMsg.Msg, req)
					assert.Nil(t, err)
					assert.Equal(t, test.metadataRequests[batch].to, req.ToUpdateVersion)
					assert.Equal(t, test.metadataRequests[batch].from, req.FromUpdateVersion)

					responseTopic := fmt.Sprintf("%s:%s", "MetadataResponse", req.CustomTopic)

					// Send response.
					for _, r := range test.metadataRequests[batch].responses {
						anyUpdates, err := types.MarshalAny(r)
						assert.Nil(t, err)
						v2cMsg := cvmsgspb.V2CMessage{
							Msg: anyUpdates,
						}
						b, err := v2cMsg.Marshal()
						assert.Nil(t, err)
						nc.Publish(vzshard.V2CTopic(responseTopic, vzID), b)
					}

					batch++
				})
				assert.Nil(t, err)
				defer mdSub.Unsubscribe()
			}

			mdr, err := controller.NewMetadataReader(db, sc, nc)
			defer mdr.Stop()

			// Publish update to STAN channel.
			initialUpdate := &metadatapb.ResourceUpdate{
				UpdateVersion:     test.updateRV,
				PrevUpdateVersion: test.updatePrevRV,
			}
			anyInitUpdate, err := types.MarshalAny(initialUpdate)
			assert.Nil(t, err)
			v2cMsg := cvmsgspb.V2CMessage{
				Msg: anyInitUpdate,
			}
			b, err := v2cMsg.Marshal()
			assert.Nil(t, err)

			sc.Publish(vzshard.V2CTopic("DurableMetadataUpdates", vzID), b)

			if len(test.expectedMetadataUpdates) > 0 {
				numUpdates := 0
				for numUpdates < len(test.expectedMetadataUpdates) {
					select {
					case idxMessage := <-idxCh:
						u := &metadatapb.ResourceUpdate{}
						err := proto.Unmarshal(idxMessage.Data, u)
						assert.Nil(t, err)
						assert.Equal(t, test.expectedMetadataUpdates[numUpdates].PrevResourceVersion, u.PrevResourceVersion)
						numUpdates++
					case <-time.After(2 * time.Second):
						t.Fatal("Timed out")
					}
				}
			} else {
				run := true
				for run {
					select {
					case _ = <-idxCh:
						t.Fatal("Unpexected index message")
					case <-time.After(2 * time.Second):
						run = false
					}
				}
			}
		})
	}
}
