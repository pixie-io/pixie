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

func TestMetadataReader_ProcessVizierUpdate(t *testing.T) {
	tests := []struct {
		name                    string                       // Name of the test
		startingRV              string                       // The resourceVersion that the metadata reader is waiting for.
		updatePrevRV            string                       // The prevResourceVersion of the update the metadata reader will receive.
		updateRV                string                       // The resourceVersion of the update the metadata reader will receive.
		expectMetadataRequest   bool                         // Whether to expect a request for missing metadata.
		metadataRequestFrom     string                       // The "from" value of the metadata req.
		metadataRequestTo       string                       // The "to" value of the metadata req.
		metadataResponse        *cvmsgspb.MetadataResponse   // The metadata response to send.
		expectedMetadataUpdates []*metadatapb.ResourceUpdate // The updates that should be sent to the indexer.
		endingRV                string                       // The new resource version after the update.
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
			endingRV: "1235",
		},
		{
			name:                  "out-of-order update",
			startingRV:            "12",
			updatePrevRV:          "16",
			updateRV:              "17",
			expectMetadataRequest: true,
			metadataRequestFrom:   "12",
			metadataRequestTo:     "17",
			metadataResponse: &cvmsgspb.MetadataResponse{
				Updates: []*metadatapb.ResourceUpdate{
					&metadatapb.ResourceUpdate{ResourceVersion: "12", PrevResourceVersion: "11"},
					&metadatapb.ResourceUpdate{ResourceVersion: "13", PrevResourceVersion: "12"},
					&metadatapb.ResourceUpdate{ResourceVersion: "15", PrevResourceVersion: "13"},
					&metadatapb.ResourceUpdate{ResourceVersion: "16", PrevResourceVersion: "15"},
				},
			},
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{
				&metadatapb.ResourceUpdate{ResourceVersion: "13", PrevResourceVersion: "12"},
				&metadatapb.ResourceUpdate{ResourceVersion: "15", PrevResourceVersion: "13"},
				&metadatapb.ResourceUpdate{ResourceVersion: "16", PrevResourceVersion: "15"},
				&metadatapb.ResourceUpdate{ResourceVersion: "17", PrevResourceVersion: "16"},
			},
			endingRV: "17",
		},
		{
			name:                    "duplicateUpdate",
			startingRV:              "12",
			updatePrevRV:            "1",
			updateRV:                "2",
			expectMetadataRequest:   false,
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{},
			endingRV:                "12",
		},
		{
			name:                  "beginningUpdate",
			startingRV:            "12",
			updatePrevRV:          "",
			updateRV:              "17",
			expectMetadataRequest: true,
			metadataRequestFrom:   "12",
			metadataRequestTo:     "17",
			metadataResponse: &cvmsgspb.MetadataResponse{
				Updates: []*metadatapb.ResourceUpdate{
					&metadatapb.ResourceUpdate{ResourceVersion: "12", PrevResourceVersion: "11"},
					&metadatapb.ResourceUpdate{ResourceVersion: "13", PrevResourceVersion: "12"},
					&metadatapb.ResourceUpdate{ResourceVersion: "15", PrevResourceVersion: "13"},
					&metadatapb.ResourceUpdate{ResourceVersion: "16", PrevResourceVersion: "15"},
				},
			},
			expectedMetadataUpdates: []*metadatapb.ResourceUpdate{
				&metadatapb.ResourceUpdate{ResourceVersion: "13", PrevResourceVersion: "12"},
				&metadatapb.ResourceUpdate{ResourceVersion: "15", PrevResourceVersion: "13"},
				&metadatapb.ResourceUpdate{ResourceVersion: "16", PrevResourceVersion: "15"},
				&metadatapb.ResourceUpdate{ResourceVersion: "17", PrevResourceVersion: ""},
			},
			endingRV: "17",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			vzID := uuid.NewV4()

			db, teardown := setupTestDB(t)
			defer teardown()

			// Set up initial DB state.
			insertIdxStateQuery := `INSERT INTO vizier_index_state(cluster_id, resource_version) VALUES ($1, $2)`
			db.MustExec(insertIdxStateQuery, vzID, test.startingRV)

			natsPort, natsCleanup := testingutils.StartNATS(t)
			nc, err := nats.Connect(testingutils.GetNATSURL(natsPort))
			if err != nil {
				t.Fatal(err)
			}
			defer natsCleanup()

			_, sc, stanCleanup := testingutils.StartStan(t, "test-stan", "test-client")
			defer stanCleanup()

			idxCh := make(chan *stan.Msg)
			indexerSub, err := sc.Subscribe("MetadataIndex", func(msg *stan.Msg) {
				idxCh <- msg
			})
			defer indexerSub.Unsubscribe()

			if test.expectMetadataRequest {
				mdSub, err := nc.Subscribe(fmt.Sprintf("c2v.%s.MetadataRequest", vzID.String()), func(msg *nats.Msg) {
					c2vMsg := &cvmsgspb.C2VMessage{}
					err := proto.Unmarshal(msg.Data, c2vMsg)
					assert.Nil(t, err)
					req := &cvmsgspb.MetadataRequest{}
					err = types.UnmarshalAny(c2vMsg.Msg, req)
					assert.Nil(t, err)
					assert.Equal(t, test.metadataRequestTo, req.To)
					assert.Equal(t, test.metadataRequestFrom, req.From)

					// Send response.
					anyUpdates, err := types.MarshalAny(test.metadataResponse)
					assert.Nil(t, err)
					v2cMsg := cvmsgspb.V2CMessage{
						Msg: anyUpdates,
					}
					b, err := v2cMsg.Marshal()
					assert.Nil(t, err)
					nc.Publish(fmt.Sprintf("v2c.%s.%s.MetadataResponse", vzshard.VizierIDToShard(vzID), vzID.String()), b)
				})
				assert.Nil(t, err)
				defer mdSub.Unsubscribe()
			}

			mr, err := controller.NewMetadataReader(db, sc, nc)
			mr.StartVizierUpdates(vzID, test.startingRV)
			defer mr.StopVizierUpdates(vzID)

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

			sc.Publish(fmt.Sprintf("v2c.%s.%s.DurableMetadataUpdates", vzshard.VizierIDToShard(vzID), vzID.String()), b)

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
