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

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/autocomplete"
	"px.dev/pixie/src/cloud/indexer/md"
	"px.dev/pixie/src/utils/testingutils/docker"
)

const indexName = "test_suggester_index"

var org1 = uuid.Must(uuid.NewV4())

var mdEntities = []md.EsMDEntity{
	{
		OrgID:              org1.String(),
		UID:                "svc1",
		Name:               "pl/testService",
		Kind:               "service",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		ClusterUID:         "test",
		UID:                "svc2",
		Name:               "anotherNS/testService",
		Kind:               "service",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		UID:                "pod1",
		Name:               "anotherNS/test-Pod",
		Kind:               "pod",
		UpdateVersion:      4,
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
		State:              md.ESMDEntityStateRunning,
	},
	{
		OrgID:              org1.String(),
		UID:                "pod12",
		Name:               "anotherNS/test-Pod",
		Kind:               "pod",
		UpdateVersion:      3,
		TimeStartedNS:      0,
		TimeStoppedNS:      1,
		RelatedEntityNames: []string{},
		State:              md.ESMDEntityStateTerminated,
	},
	{
		OrgID:              org1.String(),
		UID:                "ns1",
		Name:               "testNamespace",
		Kind:               "namespace",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		UID:                "svc3",
		Name:               "pl/abcd",
		Kind:               "service",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		UID:                "dup1",
		Name:               "dup/dup1",
		Kind:               "node",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		UID:                "dup2",
		Name:               "dup/dup2",
		Kind:               "node",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		UID:                "dup3",
		Name:               "dup/dup3",
		Kind:               "node",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		UID:                "dup4",
		Name:               "dup/dup4",
		Kind:               "node",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		UID:                "dup5",
		Name:               "dup/dup5",
		Kind:               "node",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
	{
		OrgID:              org1.String(),
		UID:                "dup6",
		Name:               "dup/dup6",
		Kind:               "node",
		TimeStartedNS:      1,
		TimeStoppedNS:      0,
		RelatedEntityNames: []string{},
	},
}

var elasticClient *elastic.Client

func TestMain(m *testing.M) {
	es, cleanup, err := docker.SetupElastic()
	if err != nil {
		cleanup()
		log.Fatal(err)
	}
	elasticClient = es

	// Set up elastic indexes.
	err = md.InitializeMapping(es, indexName, 1, "30d", "30d", false)
	if err != nil {
		cleanup()
		log.Fatal(err)
	}

	for _, e := range mdEntities {
		err = insertIntoIndex(indexName, e.UID, e)
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
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_SVC,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           false,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudpb.AEK_SVC,
						},
						{
							Name: "anotherNS/testService",
							Kind: cloudpb.AEK_SVC,
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
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_SVC,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           true,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudpb.AEK_SVC,
						},
					},
				},
			},
		},
		{
			name: "additional_matches",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input: "dup",
					OrgID: org1,
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_NODE,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           false,
					HasAdditionalMatches: true,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "dup/dup1",
							Kind: cloudpb.AEK_NODE,
						},
						{
							Name: "dup/dup2",
							Kind: cloudpb.AEK_NODE,
						},
						{
							Name: "dup/dup3",
							Kind: cloudpb.AEK_NODE,
						},
						{
							Name: "dup/dup4",
							Kind: cloudpb.AEK_NODE,
						},
						{
							Name: "dup/dup6",
							Kind: cloudpb.AEK_NODE,
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
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_SVC,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           false,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudpb.AEK_SVC,
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
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_POD,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           false,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name:  "anotherNS/test-Pod",
							Kind:  cloudpb.AEK_POD,
							State: cloudpb.AES_RUNNING,
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
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_POD,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           false,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name:  "anotherNS/test-Pod",
							Kind:  cloudpb.AEK_POD,
							State: cloudpb.AES_RUNNING,
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
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_SVC, cloudpb.AEK_POD,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           false,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudpb.AEK_SVC,
						},
						{
							Name: "anotherNS/testService",
							Kind: cloudpb.AEK_SVC,
						},
						{
							Name:  "anotherNS/test-Pod",
							Kind:  cloudpb.AEK_POD,
							State: cloudpb.AES_RUNNING,
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
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_SVC, cloudpb.AEK_POD,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           false,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "anotherNS/testService",
							Kind: cloudpb.AEK_SVC,
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
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_SVC,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
				{
					Input: "test",
					OrgID: org1,
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_SVC,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           true,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudpb.AEK_SVC,
						},
					},
				},
				{
					ExactMatch:           false,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudpb.AEK_SVC,
						},
						{
							Name: "anotherNS/testService",
							Kind: cloudpb.AEK_SVC,
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
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_SVC,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           false,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudpb.AEK_SVC,
						},
						{
							Name: "anotherNS/testService",
							Kind: cloudpb.AEK_SVC,
						},
						{
							Name: "pl/abcd",
							Kind: cloudpb.AEK_SVC,
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
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_SVC,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           false,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "pl/testService",
							Kind: cloudpb.AEK_SVC,
						},
						{
							Name: "pl/abcd",
							Kind: cloudpb.AEK_SVC,
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
		{
			name: "namespace",
			reqs: []*autocomplete.SuggestionRequest{
				{
					Input: "test",
					OrgID: org1,
					AllowedKinds: []cloudpb.AutocompleteEntityKind{
						cloudpb.AEK_NAMESPACE,
					},
					AllowedArgs: []cloudpb.AutocompleteEntityKind{},
				},
			},
			expectedResults: []*autocomplete.SuggestionResult{
				{
					ExactMatch:           false,
					HasAdditionalMatches: false,
					Suggestions: []*autocomplete.Suggestion{
						{
							Name: "testNamespace",
							Kind: cloudpb.AEK_NAMESPACE,
						},
					},
				},
			},
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			es, _ := autocomplete.NewElasticSuggester(elasticClient, indexName, "scripts", nil)
			results, err := es.GetSuggestions(test.reqs)
			require.NoError(t, err)
			assert.NotNil(t, results)
			assert.Equal(t, len(test.expectedResults), len(results))
			for i, r := range results {
				// Ensure that the expected results are atleast contained in the actual results.
				assert.GreaterOrEqual(t, len(r.Suggestions), len(test.expectedResults[i].Suggestions))
				// Remove the score so we can do a comparison.
				for j := range r.Suggestions {
					r.Suggestions[j].Score = 0
					r.Suggestions[j].MatchedIndexes = nil
				}
				// Check that the expected results are contained in the results.
				assert.Subset(t, r.Suggestions, test.expectedResults[i].Suggestions)
				assert.Equal(t, test.expectedResults[i].ExactMatch, r.ExactMatch)
				assert.Equal(t, test.expectedResults[i].HasAdditionalMatches, r.HasAdditionalMatches)
			}
		})
	}
}
