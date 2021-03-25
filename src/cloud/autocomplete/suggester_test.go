package autocomplete_test

import (
	"context"
	"log"
	"os"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/olivere/elastic/v7"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"pixielabs.ai/pixielabs/src/cloud/autocomplete"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/cloud/indexer/md"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

var org1 = uuid.Must(uuid.NewV4())

var mdEntities = []md.EsMDEntity{
	{
		OrgID:              org1.String(),
		UID:                "svc1",
		Name:               "testService",
		NS:                 "pl",
		Kind:               "service",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		ClusterUID:         "test",
		UID:                "svc2",
		Name:               "testService",
		NS:                 "anotherNS",
		Kind:               "service",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		UID:                "pod1",
		Name:               "test-Pod",
		NS:                 "anotherNS",
		Kind:               "pod",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
		State:              md.ESMDEntityStateRunning,
	},
	{
		OrgID:              org1.String(),
		UID:                "ns1",
		Name:               "testNamespace",
		NS:                 "pl",
		Kind:               "namespace",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		UID:                "svc3",
		Name:               "abcd",
		NS:                 "pl",
		Kind:               "service",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
}

var elasticClient *elastic.Client

func TestMain(m *testing.M) {
	es, cleanup, err := testingutils.SetupElastic()
	if err != nil {
		cleanup()
		log.Fatal(err)
	}
	elasticClient = es

	// Set up elastic indexes.
	_, err = es.CreateIndex(md.IndexName).Body(md.IndexMapping).Do(context.Background())
	if err != nil {
		cleanup()
		log.Fatal(err)
	}

	for _, e := range mdEntities {
		err = insertIntoIndex(md.IndexName, e.UID, e)
		if err != nil {
			cleanup()
			log.Fatal(err)
		}
	}

	code := m.Run()
	// Can't be deferred b/c of os.Exit.
	cleanup()
	os.Exit(code)
}

func insertIntoIndex(index string, id string, e md.EsMDEntity) error {
	_, err := elasticClient.Index().
		Index(index).
		Id(id).
		BodyJson(e).
		Refresh("true").
		Do(context.Background())
	if err != nil {
		return err
	}
	return nil
}

func TestGetSuggestions(t *testing.T) {
	tests := []struct {
		name            string
		input           string
		reqs            []*autocomplete.SuggestionRequest
		expectedResults []*autocomplete.SuggestionResult
	}{
		{
			name: "no namespace",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input: "test",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
						{
							Name: "anotherNS/testService",
							Kind: cloudapipb.AEK_SVC,
						},
					},
				},
			},
		},
		{
			name: "namespace",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input: "pl/testService",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch: true,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
					},
				},
			},
		},
		{
			name: "typo",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input: "pl/tss",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
					},
				},
			},
		},
		{
			name: "dash",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input: "t-Po",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_POD,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name:  "anotherNS/test-Pod",
							Kind:  cloudapipb.AEK_POD,
							State: cloudapipb.AES_RUNNING,
						},
					},
				},
			},
		},
		{
			name: "middle of name",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input: "Po",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_POD,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name:  "anotherNS/test-Pod",
							Kind:  cloudapipb.AEK_POD,
							State: cloudapipb.AES_RUNNING,
						},
					},
				},
			},
		},
		{
			name: "multiple kinds",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input: "test",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC, cloudapipb.AEK_POD,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
						{
							Name: "anotherNS/testService",
							Kind: cloudapipb.AEK_SVC,
						},
						{
							Name:  "anotherNS/test-Pod",
							Kind:  cloudapipb.AEK_POD,
							State: cloudapipb.AES_RUNNING,
						},
					},
				},
			},
		},
		{
			name: "cluster UID",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input:      "test",
					ClusterUID: "test",
					OrgID:      org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC, cloudapipb.AEK_POD,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "anotherNS/testService",
							Kind: cloudapipb.AEK_SVC,
						},
					},
				},
			},
		},
		{
			name: "multiple requests",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input: "pl/testService",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
				{
					Input: "test",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch: true,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
					},
				},
				{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
						{
							Name: "anotherNS/testService",
							Kind: cloudapipb.AEK_SVC,
						},
					},
				},
			},
		},
		{
			name: "empty",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input: "",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
						{
							Name: "anotherNS/testService",
							Kind: cloudapipb.AEK_SVC,
						},
						{
							Name: "pl/abcd",
							Kind: cloudapipb.AEK_SVC,
						},
					},
				},
			},
		},
		{
			name: "only namespace",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input: "pl/",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
						{
							Name: "pl/abcd",
							Kind: cloudapipb.AEK_SVC,
						},
					},
				},
			},
		},
		{
			name:            "empty req",
			reqs:            []*autocomplete.SuggestionRequest{},
			expectedResults: []*autocomplete.SuggestionResult{},
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			es, _ := autocomplete.NewElasticSuggester(elasticClient, "scripts", nil)
			results, err := es.GetSuggestions(test.reqs)
			require.NoError(t, err)
			assert.NotNil(t, results)
			assert.Equal(t, len(test.expectedResults), len(results))
			for i, r := range results {
				assert.Equal(t, len(test.expectedResults[i].Suggestions), len(r.Suggestions))
				// Remove the score so we can do a comparison.
				for j := range r.Suggestions {
					r.Suggestions[j].Score = 0
					r.Suggestions[j].MatchedIndexes = nil
				}
				assert.ElementsMatch(t, test.expectedResults[i].Suggestions, r.Suggestions)
				assert.Equal(t, test.expectedResults[i].ExactMatch, r.ExactMatch)
			}
		})
	}
}
