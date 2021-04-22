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

package controller_test

import (
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"px.dev/pixie/src/api/proto/cloudapipb"
	"px.dev/pixie/src/cloud/api/controller/testutils"
)

func TestAutocomplete(t *testing.T) {
	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockAutocomplete.EXPECT().Autocomplete(gomock.Any(), &cloudapipb.AutocompleteRequest{
		Input:     "px/svc_info svc:pl/test",
		CursorPos: 0,
		Action:    cloudapipb.AAT_EDIT,
	}).
		Return(&cloudapipb.AutocompleteResponse{
			FormattedInput: "${2:run} ${3:$0px/svc_info} ${1:svc:pl/test}",
			IsExecutable:   false,
			TabSuggestions: []*cloudapipb.TabSuggestion{
				{
					TabIndex:              2,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              3,
					ExecutableAfterSelect: false,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						{
							Kind:           cloudapipb.AEK_POD,
							Name:           "svc_info_pod",
							Description:    "this is a pod",
							MatchedIndexes: []int64{0, 1, 2},
							State:          cloudapipb.AES_TERMINATED,
						},
					},
				},
				{
					TabIndex:              1,
					ExecutableAfterSelect: false,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						{
							Kind:           cloudapipb.AEK_SVC,
							Name:           "pl/test",
							Description:    "this is a svc",
							MatchedIndexes: []int64{5, 6, 7},
							State:          cloudapipb.AES_RUNNING,
						},
					},
				},
			},
		}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					autocomplete(input: "px/svc_info svc:pl/test", cursorPos: 0, action: AAT_EDIT) {
						formattedInput
						isExecutable
						tabSuggestions {
							tabIndex
							executableAfterSelect
							suggestions {
								kind
								name
								description
								matchedIndexes
								state
							}
						}
					}
				}
			`,
			ExpectedResult: `
				{
					"autocomplete": {
						"formattedInput": "${2:run} ${3:$0px/svc_info} ${1:svc:pl/test}",
						"isExecutable": false,
						"tabSuggestions": [
							{ "tabIndex": 2, "executableAfterSelect": false, "suggestions": []},
							{ "tabIndex": 3, "executableAfterSelect": false, "suggestions":
								[{"kind": "AEK_POD", "name": "svc_info_pod", "description": "this is a pod", "matchedIndexes": [0, 1, 2], "state": "AES_TERMINATED"}]
							},
							{ "tabIndex": 1, "executableAfterSelect": false, "suggestions":
								[{"kind": "AEK_SVC", "name": "pl/test", "description": "this is a svc", "matchedIndexes": [5, 6, 7], "state": "AES_RUNNING"}]
							}
						]
					}
				}
			`,
		},
	})
}

func TestAutocompleteField(t *testing.T) {
	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockAutocomplete.EXPECT().AutocompleteField(gomock.Any(), &cloudapipb.AutocompleteFieldRequest{
		Input:            "px/svc_info",
		FieldType:        cloudapipb.AEK_SVC,
		RequiredArgTypes: []cloudapipb.AutocompleteEntityKind{},
		ClusterUID:       "test",
	}).
		Return(&cloudapipb.AutocompleteFieldResponse{
			Suggestions: []*cloudapipb.AutocompleteSuggestion{
				{
					Kind:           cloudapipb.AEK_SVC,
					Name:           "px/svc_info",
					Description:    "test",
					MatchedIndexes: []int64{0, 1, 2},
					State:          cloudapipb.AES_TERMINATED,
				},
				{
					Kind:           cloudapipb.AEK_SVC,
					Name:           "px/svc_info2",
					Description:    "test2",
					MatchedIndexes: []int64{0, 1, 2},
					State:          cloudapipb.AES_RUNNING,
				},
			},
		}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					autocompleteField(input: "px/svc_info", fieldType: AEK_SVC, clusterUID: "test") {
						kind
						name
						description
						matchedIndexes
						state
					}
				}
			`,
			ExpectedResult: `
				{
					"autocompleteField":
						[
						 {"kind": "AEK_SVC", "name": "px/svc_info", "description": "test", "matchedIndexes": [0, 1, 2], "state": "AES_TERMINATED"},
						 {"kind": "AEK_SVC", "name": "px/svc_info2", "description": "test2", "matchedIndexes": [0, 1, 2], "state": "AES_RUNNING"}
						]
				}
			`,
		},
	})
}
