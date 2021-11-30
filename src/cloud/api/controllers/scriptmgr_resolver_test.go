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
	"reflect"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/vispb"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
)

func TestScriptMgrResolver(t *testing.T) {
	testCases := []struct {
		name           string
		endpoint       string
		req            proto.Message
		resp           proto.Message
		query          string
		expectedResult string
		variables      map[string]interface{}
	}{
		{
			name:     "GetLiveViews returns correct graphql",
			endpoint: "GetLiveViews",
			req:      &cloudpb.GetLiveViewsReq{},
			resp: &cloudpb.GetLiveViewsResp{
				LiveViews: []*cloudpb.LiveViewMetadata{
					{
						ID:   "test-id-1",
						Name: "1",
						Desc: "1 desc",
					},
					{
						ID:   "test-id-2",
						Name: "2",
						Desc: "2 desc",
					},
				},
			},
			query: `
				query {
					liveViews {
						id
						name
						desc
					}
				}
			`,
			expectedResult: `
				{
					"liveViews": [
						{
							"id": "test-id-1",
							"name": "1",
							"desc": "1 desc"
						},
						{
							"id": "test-id-2",
							"name": "2",
							"desc": "2 desc"
						}
					]
				}
			`,
			variables: map[string]interface{}{},
		},
		{
			name:     "GetLiveViewContents returns correct graphql",
			endpoint: "GetLiveViewContents",
			req: &cloudpb.GetLiveViewContentsReq{
				LiveViewID: "test-id-1",
			},
			resp: &cloudpb.GetLiveViewContentsResp{
				Metadata: &cloudpb.LiveViewMetadata{
					ID:   "test-id-1",
					Name: "1",
					Desc: "1 desc",
				},
				PxlContents: "1 pxl",
				Vis: &vispb.Vis{
					Widgets: []*vispb.Widget{
						{
							FuncOrRef: &vispb.Widget_Func_{
								Func: &vispb.Widget_Func{
									Name: "my_func",
								},
							},
							DisplaySpec: toAny(t, &vispb.VegaChart{
								Spec: "{}",
							}),
						},
					},
				},
			},
			query: `
				query LiveViewContents($id: ID!) {
					liveViewContents(id: $id) {
						metadata {
							id
							name
							desc
						},
						pxlContents
						visJSON
					}
				}
			`,
			expectedResult: `
				{
					"liveViewContents": {
						"metadata": {
							"id": "test-id-1",
							"name": "1",
							"desc": "1 desc"
						},
						"pxlContents": "1 pxl",
						"visJSON": "{\"widgets\":[{\"func\":{\"name\":\"my_func\"},\"displaySpec\":` +
				`{\"@type\":\"type.googleapis.com/px.vispb.VegaChart\",\"spec\":\"{}\"}}]}"
					}
				}
			`,
			variables: map[string]interface{}{
				"id": "test-id-1",
			},
		},
		{
			name:     "GetScripts returns correct graphql",
			endpoint: "GetScripts",
			req:      &cloudpb.GetScriptsReq{},
			resp: &cloudpb.GetScriptsResp{
				Scripts: []*cloudpb.ScriptMetadata{
					{
						ID:          "test-id-1",
						Name:        "1",
						Desc:        "1 desc",
						HasLiveView: false,
					},
					{
						ID:          "test-id-2",
						Name:        "2",
						Desc:        "2 desc",
						HasLiveView: true,
					},
				},
			},
			query: `
				query {
					scripts {
						id
						name
						desc
						hasLiveView
					}
				}
			`,
			expectedResult: `
				{
					"scripts": [
						{
							"id": "test-id-1",
							"name": "1",
							"desc": "1 desc",
							"hasLiveView": false
						},
						{
							"id": "test-id-2",
							"name": "2",
							"desc": "2 desc",
							"hasLiveView": true
						}
					]
				}
			`,
			variables: map[string]interface{}{},
		},
		{
			name:     "GetScriptContents returns correct graphql",
			endpoint: "GetScriptContents",
			req: &cloudpb.GetScriptContentsReq{
				ScriptID: "test-id-1",
			},
			resp: &cloudpb.GetScriptContentsResp{
				Metadata: &cloudpb.ScriptMetadata{
					ID:          "test-id-1",
					Name:        "1",
					Desc:        "1 desc",
					HasLiveView: false,
				},
				Contents: "1 pxl",
			},
			query: `
				query ScriptContents($id: ID!) {
					scriptContents(id: $id) {
						metadata {
							id
							name
							desc
							hasLiveView
						},
						contents
					}
				}
			`,
			expectedResult: `
				{
					"scriptContents": {
						"metadata": {
							"id": "test-id-1",
							"name": "1",
							"desc": "1 desc",
							"hasLiveView": false
						},
						"contents": "1 pxl"
					}
				}
			`,
			variables: map[string]interface{}{
				"id": "test-id-1",
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := context.Background()

			reflect.ValueOf(mockClients.MockScriptMgr.EXPECT()).
				MethodByName(tc.endpoint).
				Call([]reflect.Value{
					reflect.ValueOf(gomock.Any()),
					reflect.ValueOf(tc.req),
				})[0].Interface().(*gomock.Call).
				Return(tc.resp, nil)
			gqlSchema := LoadSchema(gqlEnv)
			gqltesting.RunTests(t, []*gqltesting.Test{
				{
					Schema:         gqlSchema,
					Context:        ctx,
					Query:          tc.query,
					Variables:      tc.variables,
					ExpectedResult: tc.expectedResult,
				},
			})
		})
	}
}
