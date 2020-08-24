package md_test

import (
	"context"
	"encoding/json"
	"os"
	"testing"

	"github.com/olivere/elastic/v7"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/indexer/md"
	mdpb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

var elasticClient *elastic.Client
var vzID uuid.UUID
var orgID uuid.UUID

func TestMain(m *testing.M) {
	es, cleanup := testingutils.SetupElastic()
	elasticClient = es
	vzID = uuid.NewV4()
	orgID = uuid.NewV4()

	err := md.InitializeMapping(es)
	if err != nil {
		log.WithError(err).Fatal("Could not initialize indexes in elastic")
	}

	code := m.Run()
	// Can't be deferred b/c of os.Exit.
	cleanup()
	os.Exit(code)
}

func TestVizierIndexer_ResourceUpdate(t *testing.T) {
	tests := []struct {
		name            string
		updates         []*mdpb.ResourceUpdate
		updateKind      string
		expectedResults []*md.EsMDEntity
	}{
		{
			name: "namespace update",
			updates: []*mdpb.ResourceUpdate{
				&mdpb.ResourceUpdate{
					Update: &mdpb.ResourceUpdate_NamespaceUpdate{
						NamespaceUpdate: &mdpb.NamespaceUpdate{
							UID:              "100",
							Name:             "testns",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
						},
					},
					ResourceVersion:     "0",
					PrevResourceVersion: "",
				},
			},
			updateKind: "namespace",
			expectedResults: []*md.EsMDEntity{
				&md.EsMDEntity{
					OrgID:              orgID.String(),
					VizierID:           vzID.String(),
					ClusterUID:         "test",
					UID:                "100",
					NS:                 "testns",
					Name:               "testns",
					Kind:               "namespace",
					TimeStartedNS:      int64(1000),
					TimeStoppedNS:      int64(0),
					RelatedEntityNames: []string{},
					ResourceVersion:    "0",
				},
			},
		},
		{
			name: "pod update",
			updates: []*mdpb.ResourceUpdate{
				&mdpb.ResourceUpdate{
					Update: &mdpb.ResourceUpdate_PodUpdate{
						PodUpdate: &mdpb.PodUpdate{
							UID:              "300",
							Name:             "test-pod",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
						},
					},
					ResourceVersion:     "2",
					PrevResourceVersion: "1",
				},
			},
			updateKind: "pod",
			expectedResults: []*md.EsMDEntity{
				&md.EsMDEntity{
					OrgID:              orgID.String(),
					VizierID:           vzID.String(),
					ClusterUID:         "test",
					UID:                "300",
					NS:                 "",
					Name:               "test-pod",
					Kind:               "pod",
					TimeStartedNS:      int64(1000),
					TimeStoppedNS:      int64(0),
					RelatedEntityNames: []string{},
					ResourceVersion:    "2",
				},
			},
		},
		{
			name: "svc update",
			updates: []*mdpb.ResourceUpdate{
				&mdpb.ResourceUpdate{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
						},
					},
					ResourceVersion:     "1",
					PrevResourceVersion: "0",
				},
			},
			updateKind: "service",
			expectedResults: []*md.EsMDEntity{
				&md.EsMDEntity{
					OrgID:              orgID.String(),
					VizierID:           vzID.String(),
					ClusterUID:         "test",
					UID:                "200",
					NS:                 "",
					Name:               "test-service",
					Kind:               "service",
					TimeStartedNS:      int64(1000),
					TimeStoppedNS:      int64(0),
					RelatedEntityNames: []string{},
					ResourceVersion:    "1",
				},
			},
		},
		{
			name: "out of order svc update",
			updates: []*mdpb.ResourceUpdate{
				&mdpb.ResourceUpdate{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
						},
					},
					ResourceVersion:     "2",
					PrevResourceVersion: "1",
				},
				&mdpb.ResourceUpdate{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
							PodIDs:           []string{"abcd"},
						},
					},
					ResourceVersion:     "0",
					PrevResourceVersion: "",
				},
			},
			updateKind: "service",
			expectedResults: []*md.EsMDEntity{
				&md.EsMDEntity{
					OrgID:              orgID.String(),
					VizierID:           vzID.String(),
					ClusterUID:         "test",
					UID:                "200",
					NS:                 "",
					Name:               "test-service",
					Kind:               "service",
					TimeStartedNS:      int64(1000),
					TimeStoppedNS:      int64(0),
					RelatedEntityNames: []string{},
					ResourceVersion:    "2",
				},
			},
		},
		{
			name: "in order svc update",
			updates: []*mdpb.ResourceUpdate{
				&mdpb.ResourceUpdate{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
						},
					},
					ResourceVersion:     "2",
					PrevResourceVersion: "1",
				},
				&mdpb.ResourceUpdate{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
							PodIDs:           []string{"efgh"},
						},
					},
					ResourceVersion:     "3",
					PrevResourceVersion: "2",
				},
				&mdpb.ResourceUpdate{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  1200,
							PodIDs:           []string{"efgh", "abcd"},
						},
					},
					ResourceVersion:     "4",
					PrevResourceVersion: "3",
				},
			},
			updateKind: "service",
			expectedResults: []*md.EsMDEntity{
				&md.EsMDEntity{
					OrgID:              orgID.String(),
					VizierID:           vzID.String(),
					ClusterUID:         "test",
					UID:                "200",
					NS:                 "",
					Name:               "test-service",
					Kind:               "service",
					TimeStartedNS:      int64(1000),
					TimeStoppedNS:      int64(1200),
					RelatedEntityNames: []string{"abcd", "efgh"},
					ResourceVersion:    "4",
				},
			},
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			indexer := md.NewVizierIndexer(vzID, orgID, "test", nil, elasticClient)

			for _, u := range test.updates {
				err := indexer.HandleResourceUpdate(u)
				assert.Nil(t, err)
			}

			resp, err := elasticClient.Search().
				Index("md_entities_3").
				Query(elastic.NewTermQuery("kind", test.updateKind)).
				Do(context.Background())
			assert.Nil(t, err)
			assert.Equal(t, int64(len(test.expectedResults)), resp.TotalHits())
			for i, r := range test.expectedResults {
				res := &md.EsMDEntity{}
				err = json.Unmarshal(resp.Hits.Hits[i].Source, res)
				assert.Nil(t, err)
				assert.Equal(t, r, res)
			}
		})
	}
}
