package controller_test

import (
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
	from      string
	to        string
	responses []*cvmsgspb.MetadataResponse
}

func TestMetadataReader_ProcessVizierUpdate(t *testing.T) {
	tests := []struct {
		name                    string                       // Name of the test
		startingRV              string                       // The resourceVersion that the metadata reader is waiting for.
		updatePrevRV            string                       // The prevResourceVersion of the update the metadata reader will receive.
		updateRV                string                       // The resourceVersion of the update the metadata reader will receive.
		expectMetadataRequest   bool                         // Whether to expect a request for missing metadata.
		metadataRequests        []*MetadataRequest           // The expected metadata request and responses.
		expectedMetadataUpdates []*metadatapb.ResourceUpdate // The updates that should be sent to the indexer.
		endingRV                string                       // The new resource version after the update.
		vizierStatus            string
	}{
		{
			name:                  "In-order update",
			startingRV:            "1234",
			updatePrevRV:          "1234",
			updateRV:              "1235",
			expectMetadataRequest: false,
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{
				&metadatapb.ResourceUpdate{ResourceVersion: "1235", PrevResourceVersion: "1234"},
			},
			endingRV:     "1235",
			vizierStatus: "HEALTHY",
		},
		{
			name:                  "out-of-order update",
			startingRV:            "12",
			updatePrevRV:          "16",
			updateRV:              "17",
			expectMetadataRequest: true,
			metadataRequests: []*MetadataRequest{
				&MetadataRequest{
					from: "12",
					to:   "17",
					responses: []*cvmsgspb.MetadataResponse{
						&cvmsgspb.MetadataResponse{
							Updates: []*metadatapb.ResourceUpdate{
								&metadatapb.ResourceUpdate{ResourceVersion: "12", PrevResourceVersion: "11"},
								&metadatapb.ResourceUpdate{ResourceVersion: "13", PrevResourceVersion: "12"},
								&metadatapb.ResourceUpdate{ResourceVersion: "15", PrevResourceVersion: "13"},
								&metadatapb.ResourceUpdate{ResourceVersion: "16", PrevResourceVersion: "15"},
							},
						},
					},
				},
			},
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{
				&metadatapb.ResourceUpdate{ResourceVersion: "13", PrevResourceVersion: "12"},
				&metadatapb.ResourceUpdate{ResourceVersion: "15", PrevResourceVersion: "13"},
				&metadatapb.ResourceUpdate{ResourceVersion: "16", PrevResourceVersion: "15"},
				&metadatapb.ResourceUpdate{ResourceVersion: "17", PrevResourceVersion: "16"},
			},
			endingRV:     "17",
			vizierStatus: "UNHEALTHY",
		},
		{
			name:                  "multiple out-of-order update",
			startingRV:            "12",
			updatePrevRV:          "16",
			updateRV:              "17",
			expectMetadataRequest: true,
			metadataRequests: []*MetadataRequest{
				&MetadataRequest{
					from: "12",
					to:   "17",
					responses: []*cvmsgspb.MetadataResponse{
						&cvmsgspb.MetadataResponse{
							Updates: []*metadatapb.ResourceUpdate{
								&metadatapb.ResourceUpdate{ResourceVersion: "12", PrevResourceVersion: "11"},
								&metadatapb.ResourceUpdate{ResourceVersion: "13", PrevResourceVersion: "12"},
							},
						},
						&cvmsgspb.MetadataResponse{
							Updates: []*metadatapb.ResourceUpdate{
								&metadatapb.ResourceUpdate{ResourceVersion: "16", PrevResourceVersion: "15"},
							},
						},
					},
				},
				&MetadataRequest{
					from: "13",
					to:   "17",
					responses: []*cvmsgspb.MetadataResponse{
						&cvmsgspb.MetadataResponse{
							Updates: []*metadatapb.ResourceUpdate{
								&metadatapb.ResourceUpdate{ResourceVersion: "15", PrevResourceVersion: "13"},
								&metadatapb.ResourceUpdate{ResourceVersion: "16", PrevResourceVersion: "15"},
							},
						},
					},
				},
			},
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{
				&metadatapb.ResourceUpdate{ResourceVersion: "13", PrevResourceVersion: "12"},
				&metadatapb.ResourceUpdate{ResourceVersion: "15", PrevResourceVersion: "13"},
				&metadatapb.ResourceUpdate{ResourceVersion: "16", PrevResourceVersion: "15"},
				&metadatapb.ResourceUpdate{ResourceVersion: "17", PrevResourceVersion: "16"},
			},
			endingRV:     "17",
			vizierStatus: "HEALTHY",
		},
		{
			name:                    "disconnected vizier",
			startingRV:              "12",
			updatePrevRV:            "1",
			updateRV:                "2",
			expectMetadataRequest:   false,
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{},
			endingRV:                "12",
			vizierStatus:            "DISCONNECTED",
		},
		{
			name:                    "duplicateUpdate",
			startingRV:              "12",
			updatePrevRV:            "1",
			updateRV:                "2",
			expectMetadataRequest:   false,
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{},
			endingRV:                "12",
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
			insertIdxStateQuery := `INSERT INTO vizier_index_state(cluster_id, resource_version) VALUES ($1, $2)`
			db.MustExec(insertIdxStateQuery, vzID, test.startingRV)
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
					req := &cvmsgspb.MetadataRequest{}
					err = types.UnmarshalAny(c2vMsg.Msg, req)
					assert.Nil(t, err)
					assert.Equal(t, test.metadataRequests[batch].to, req.To)
					assert.Equal(t, test.metadataRequests[batch].from, req.From)

					// Send response.
					for _, r := range test.metadataRequests[batch].responses {
						anyUpdates, err := types.MarshalAny(r)
						assert.Nil(t, err)
						v2cMsg := cvmsgspb.V2CMessage{
							Msg: anyUpdates,
						}
						b, err := v2cMsg.Marshal()
						assert.Nil(t, err)
						nc.Publish(vzshard.V2CTopic(req.Topic, vzID), b)
					}

					batch++
				})
				assert.Nil(t, err)
				defer mdSub.Unsubscribe()
			}

			mdr, err := controller.NewMetadataReader(db, sc, nc)
			defer mdr.Stop()

			// Publish update to STAN channel.
			initialUpdate := &cvmsgspb.MetadataUpdate{
				Update: &metadatapb.ResourceUpdate{
					ResourceVersion:     test.updateRV,
					PrevResourceVersion: test.updatePrevRV,
				},
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
