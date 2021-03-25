package controller_test

import (
	"context"
	"reflect"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	pl_vispb "pixielabs.ai/pixielabs/src/shared/vispb"
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
			req:      &cloudapipb.GetLiveViewsReq{},
			resp: &cloudapipb.GetLiveViewsResp{
				LiveViews: []*cloudapipb.LiveViewMetadata{
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
			req: &cloudapipb.GetLiveViewContentsReq{
				LiveViewID: "test-id-1",
			},
			resp: &cloudapipb.GetLiveViewContentsResp{
				Metadata: &cloudapipb.LiveViewMetadata{
					ID:   "test-id-1",
					Name: "1",
					Desc: "1 desc",
				},
				PxlContents: "1 pxl",
				Vis: &pl_vispb.Vis{
					Widgets: []*pl_vispb.Widget{
						{
							FuncOrRef: &pl_vispb.Widget_Func_{
								Func: &pl_vispb.Widget_Func{
									Name: "my_func",
								},
							},
							DisplaySpec: &types.Any{
								TypeUrl: "pixielabs.ai/pl.vispb.VegaChart",
								Value: toBytes(t, &pl_vispb.VegaChart{
									Spec: "{}",
								}),
							},
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
				`{\"@type\":\"pixielabs.ai/pl.vispb.VegaChart\",\"spec\":\"{}\"}}]}"
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
			req:      &cloudapipb.GetScriptsReq{},
			resp: &cloudapipb.GetScriptsResp{
				Scripts: []*cloudapipb.ScriptMetadata{
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
			req: &cloudapipb.GetScriptContentsReq{
				ScriptID: "test-id-1",
			},
			resp: &cloudapipb.GetScriptContentsResp{
				Metadata: &cloudapipb.ScriptMetadata{
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
