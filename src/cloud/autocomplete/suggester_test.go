package autocomplete_test

import (
	"context"
	"os"
	"testing"

	"github.com/olivere/elastic/v7"
	uuid "github.com/satori/go.uuid"

	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/cloud/autocomplete"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

var org1 uuid.UUID = uuid.NewV4()

// We should consider making these indices a shared file between this test and the indexer service.
const mdIndexMapping = `
{
    "settings":{
      "number_of_shards":1,
      "number_of_replicas":0,
        "analysis": {
          "filter": {
            "autocomplete_filter": {
              "type": "edge_ngram",
              "min_gram": 1,
              "max_gram": 20
            },
            "dont_split_on_numerics" : {
              "type" : "word_delimiter",
              "preserve_original": true,
              "generate_number_parts" : false
            }
          },
          "tokenizer": {
            "my_tokenizer": {
              "type": "pattern",
              "pattern": "-"
            }
          },
          "analyzer": {
            "autocomplete": {
              "type": "custom",
              "tokenizer": "my_tokenizer",
              "filter": [
                "lowercase",
                "autocomplete_filter"
              ]
            },
            "myAnalyzer" : {
              "type" : "custom",
              "tokenizer" : "whitespace",
              "filter" : [ "dont_split_on_numerics" ]
            }
          }
        }
    },
  "mappings":{
    "properties":{
    "orgID":{
      "type":"text", "analyzer": "myAnalyzer"
    },
    "uid":{
      "type":"text"
    },
    "name":{
      "type":"text",
        "analyzer": "autocomplete"
    },
    "ns":{
      "type":"text"
    },
    "kind":{
      "type":"text"
    },
    "timeStartedNS":{
      "type":"long"
    },
    "timeStoppedNS":{
      "type":"long"
    },
    "relatedEntityNames":{
      "type":"text"
    },
    "ResourceVersion":{
      "type":"text"
    }
    }
  }
}
`

var mdEntities = []autocomplete.EsMDEntity{
	autocomplete.EsMDEntity{
		OrgID:              org1.String(),
		UID:                "svc1",
		Name:               "testService",
		NS:                 "pl",
		Kind:               "service",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	autocomplete.EsMDEntity{
		OrgID:              org1.String(),
		UID:                "svc2",
		Name:               "testService",
		NS:                 "anotherNS",
		Kind:               "service",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	autocomplete.EsMDEntity{
		OrgID:              org1.String(),
		UID:                "pod1",
		Name:               "testPod",
		NS:                 "anotherNS",
		Kind:               "pod",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	autocomplete.EsMDEntity{
		OrgID:              org1.String(),
		UID:                "ns1",
		Name:               "testNamespace",
		NS:                 "pl",
		Kind:               "namespace",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	autocomplete.EsMDEntity{
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
	es, cleanup := testingutils.SetupElastic()
	elasticClient = es

	// Set up elastic indexes.
	_, err := es.CreateIndex("md_entities").Body(mdIndexMapping).Do(context.Background())
	if err != nil {
		panic(err)
	}

	for _, e := range mdEntities {
		err = insertIntoIndex("md_entities", e.UID, e)
		if err != nil {
			panic(err)
		}
	}

	code := m.Run()
	// Can't be deferred b/c of os.Exit.
	cleanup()
	os.Exit(code)
}

func insertIntoIndex(index string, id string, e autocomplete.EsMDEntity) error {
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
				&autocomplete.SuggestionRequest{
					Input: "test",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				&autocomplete.SuggestionResult{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
						&autocomplete.Suggestion{
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
				&autocomplete.SuggestionRequest{
					Input: "pl/testService",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				&autocomplete.SuggestionResult{
					ExactMatch: true,
					Suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
					},
				},
			},
		},
		{
			name: "multiple kinds",
			reqs: []*autocomplete.SuggestionRequest{
				&autocomplete.SuggestionRequest{
					Input: "test",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC, cloudapipb.AEK_POD,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				&autocomplete.SuggestionResult{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
						&autocomplete.Suggestion{
							Name: "anotherNS/testService",
							Kind: cloudapipb.AEK_SVC,
						},
						&autocomplete.Suggestion{
							Name: "anotherNS/testPod",
							Kind: cloudapipb.AEK_POD,
						},
					},
				},
			},
		},
		{
			name: "multiple requests",
			reqs: []*autocomplete.SuggestionRequest{
				&autocomplete.SuggestionRequest{
					Input: "pl/testService",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
				&autocomplete.SuggestionRequest{
					Input: "test",
					OrgID: org1,
					AllowedKinds: []cloudapipb.AutocompleteEntityKind{
						cloudapipb.AEK_SVC,
					},
					AllowedArgs: []cloudapipb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				&autocomplete.SuggestionResult{
					ExactMatch: true,
					Suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
					},
				},
				&autocomplete.SuggestionResult{
					ExactMatch: false,
					Suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name: "pl/testService",
							Kind: cloudapipb.AEK_SVC,
						},
						&autocomplete.Suggestion{
							Name: "anotherNS/testService",
							Kind: cloudapipb.AEK_SVC,
						},
					},
				},
			},
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			es := autocomplete.NewElasticSuggester(elasticClient, "md_entities", "scripts")
			results, err := es.GetSuggestions(test.reqs)
			assert.Nil(t, err)
			assert.NotNil(t, results)
			assert.Equal(t, len(test.expectedResults), len(results))
			for i, r := range results {
				assert.Equal(t, len(test.expectedResults[i].Suggestions), len(r.Suggestions))
				// Remove the score so we can do a comparison.
				for j := range r.Suggestions {
					r.Suggestions[j].Score = 0
				}
				assert.ElementsMatch(t, test.expectedResults[i].Suggestions, r.Suggestions)
				assert.Equal(t, test.expectedResults[i].ExactMatch, r.ExactMatch)
			}
		})
	}
}
