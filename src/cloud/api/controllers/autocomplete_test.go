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
	"context"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/autocomplete"
	mock_autocomplete "px.dev/pixie/src/cloud/autocomplete/mock"
)

func TestAutocompleteService_Autocomplete(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			orgID, err := uuid.FromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
			require.NoError(t, err)
			ctx := test.ctx

			s := mock_autocomplete.NewMockSuggester(ctrl)

			requests := [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudpb.AutocompleteEntityKind{cloudpb.AEK_POD, cloudpb.AEK_SVC, cloudpb.AEK_NAMESPACE, cloudpb.AEK_SCRIPT},
						AllowedArgs:  []cloudpb.AutocompleteEntityKind{},
					},
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "pl/test",
						AllowedKinds: []cloudpb.AutocompleteEntityKind{cloudpb.AEK_POD, cloudpb.AEK_SVC, cloudpb.AEK_NAMESPACE, cloudpb.AEK_SCRIPT},
						AllowedArgs:  []cloudpb.AutocompleteEntityKind{},
					},
				},
			}

			responses := [][]*autocomplete.SuggestionResult{
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:     "px/svc_info",
								Score:    1,
								ArgNames: []string{"svc_name"},
								ArgKinds: []cloudpb.AutocompleteEntityKind{cloudpb.AEK_SVC},
							},
						},
						ExactMatch: true,
					},
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "px/test",
								Score: 1,
							},
						},
						ExactMatch: true,
					},
				},
			}

			suggestionCalls := 0
			s.EXPECT().
				GetSuggestions(gomock.Any()).
				DoAndReturn(func(req []*autocomplete.SuggestionRequest) ([]*autocomplete.SuggestionResult, error) {
					assert.ElementsMatch(t, requests[suggestionCalls], req)
					resp := responses[suggestionCalls]
					suggestionCalls++
					return resp, nil
				}).
				Times(len(requests))

			autocompleteServer := &controllers.AutocompleteServer{
				Suggester: s,
			}

			resp, err := autocompleteServer.Autocomplete(ctx, &cloudpb.AutocompleteRequest{
				Input:      "px/svc_info pl/test",
				CursorPos:  0,
				Action:     cloudpb.AAT_EDIT,
				ClusterUID: "test",
			})
			require.NoError(t, err)
			assert.NotNil(t, resp)
			assert.Equal(t, "${2:$0px/svc_info} ${1:pl/test}", resp.FormattedInput)
			assert.False(t, resp.IsExecutable)
			assert.Equal(t, 2, len(resp.TabSuggestions))
		})
	}
}

func TestAutocompleteService_AutocompleteField(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			orgID, err := uuid.FromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
			require.NoError(t, err)
			ctx := test.ctx

			s := mock_autocomplete.NewMockSuggester(ctrl)

			requests := [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudpb.AutocompleteEntityKind{cloudpb.AEK_SVC},
						AllowedArgs:  []cloudpb.AutocompleteEntityKind{},
					},
				},
			}

			responses := []*autocomplete.SuggestionResult{
				{
					Suggestions: []*autocomplete.Suggestion{
						{
							Name:  "px/svc_info",
							Score: 1,
							State: cloudpb.AES_RUNNING,
						},
						{
							Name:  "px/svc_info2",
							Score: 1,
							State: cloudpb.AES_TERMINATED,
						},
					},
					ExactMatch: true,
				},
			}

			s.EXPECT().
				GetSuggestions(gomock.Any()).
				DoAndReturn(func(req []*autocomplete.SuggestionRequest) ([]*autocomplete.SuggestionResult, error) {
					assert.ElementsMatch(t, requests[0], req)
					return responses, nil
				})

			autocompleteServer := &controllers.AutocompleteServer{
				Suggester: s,
			}

			resp, err := autocompleteServer.AutocompleteField(ctx, &cloudpb.AutocompleteFieldRequest{
				Input:      "px/svc_info",
				FieldType:  cloudpb.AEK_SVC,
				ClusterUID: "test",
			})
			require.NoError(t, err)
			assert.NotNil(t, resp)
			assert.Equal(t, 2, len(resp.Suggestions))
		})
	}
}
