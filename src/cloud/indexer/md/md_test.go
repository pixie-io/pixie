package md_test

import (
	"context"
	"encoding/json"
	"os"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/olivere/elastic/v7"
	log "github.com/sirupsen/logrus"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"pixielabs.ai/pixielabs/src/cloud/indexer/md"
	mdpb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

var elasticClient *elastic.Client
var vzID uuid.UUID
var orgID uuid.UUID

func TestMain(m *testing.M) {
	es, cleanup, err := testingutils.SetupElastic()
	if err != nil {
		cleanup()
		log.Fatal(err)
	}

	vzID = uuid.Must(uuid.NewV4())
	orgID = uuid.Must(uuid.NewV4())

	err = md.InitializeMapping(es)
	if err != nil {
		cleanup()
		log.WithError(err).Fatal("Could not initialize indexes in elastic")
	}

	elasticClient = es
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
				{
					Update: &mdpb.ResourceUpdate_NamespaceUpdate{
						NamespaceUpdate: &mdpb.NamespaceUpdate{
							UID:              "100",
							Name:             "testns",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
						},
					},
					UpdateVersion: 0,
				},
			},
			updateKind: "namespace",
			expectedResults: []*md.EsMDEntity{
				{
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
					UpdateVersion:      0,
					State:              md.ESMDEntityStateRunning,
				},
			},
		},
		{
			name: "pod update",
			updates: []*mdpb.ResourceUpdate{
				{
					Update: &mdpb.ResourceUpdate_PodUpdate{
						PodUpdate: &mdpb.PodUpdate{
							UID:              "300",
							Name:             "test-pod",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
							Phase:            mdpb.PENDING,
						},
					},
					UpdateVersion:     2,
					PrevUpdateVersion: 1,
				},
			},
			updateKind: "pod",
			expectedResults: []*md.EsMDEntity{
				{
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
					UpdateVersion:      2,
					State:              md.ESMDEntityStatePending,
				},
			},
		},
		{
			name: "svc update",
			updates: []*mdpb.ResourceUpdate{
				{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
						},
					},
					UpdateVersion:     1,
					PrevUpdateVersion: 0,
				},
			},
			updateKind: "service",
			expectedResults: []*md.EsMDEntity{
				{
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
					UpdateVersion:      1,
					State:              md.ESMDEntityStateRunning,
				},
			},
		},
		{
			name: "out of order svc update",
			updates: []*mdpb.ResourceUpdate{
				{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
						},
					},
					UpdateVersion:     2,
					PrevUpdateVersion: 1,
				},
				{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
							PodIDs:           []string{"abcd"},
						},
					},
					UpdateVersion: 0,
				},
			},
			updateKind: "service",
			expectedResults: []*md.EsMDEntity{
				{
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
					UpdateVersion:      2,
					State:              md.ESMDEntityStateRunning,
				},
			},
		},
		{
			name: "in order svc update",
			updates: []*mdpb.ResourceUpdate{
				{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
						},
					},
					UpdateVersion:     2,
					PrevUpdateVersion: 1,
				},
				{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  0,
							PodIDs:           []string{"efgh"},
						},
					},
					UpdateVersion:     3,
					PrevUpdateVersion: 2,
				},
				{
					Update: &mdpb.ResourceUpdate_ServiceUpdate{
						ServiceUpdate: &mdpb.ServiceUpdate{
							UID:              "200",
							Name:             "test-service",
							StartTimestampNS: 1000,
							StopTimestampNS:  1200,
							PodIDs:           []string{"efgh", "abcd"},
						},
					},
					UpdateVersion:     4,
					PrevUpdateVersion: 3,
				},
			},
			updateKind: "service",
			expectedResults: []*md.EsMDEntity{
				{
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
					UpdateVersion:      4,
					State:              md.ESMDEntityStateTerminated,
				},
			},
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			indexer := md.NewVizierIndexer(vzID, orgID, "test", nil, elasticClient)

			for _, u := range test.updates {
				err := indexer.HandleResourceUpdate(u)
				require.NoError(t, err)
			}

			resp, err := elasticClient.Search().
				Index(md.IndexName).
				Query(elastic.NewTermQuery("kind", test.updateKind)).
				Do(context.Background())
			require.NoError(t, err)
			assert.Equal(t, int64(len(test.expectedResults)), resp.TotalHits())
			for i, r := range test.expectedResults {
				res := &md.EsMDEntity{}
				err = json.Unmarshal(resp.Hits.Hits[i].Source, res)
				require.NoError(t, err)
				assert.Equal(t, r, res)
			}
		})
	}
}
