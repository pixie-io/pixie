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

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/vispb"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/scriptmgr/scriptmgrpb"
	mock_scriptmgr "px.dev/pixie/src/cloud/scriptmgr/scriptmgrpb/mock"
	"px.dev/pixie/src/utils"
)

func toAny(t *testing.T, msg proto.Message) *types.Any {
	anyMsg, err := types.MarshalAny(msg)
	require.NoError(t, err)
	return anyMsg
}

func TestScriptMgr(t *testing.T) {
	var testVis = &vispb.Vis{
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
	}

	ID1 := uuid.Must(uuid.NewV4())
	ID2 := uuid.Must(uuid.NewV4())
	testCases := []struct {
		name         string
		endpoint     string
		smReq        proto.Message
		smResp       proto.Message
		req          proto.Message
		expectedResp proto.Message
		ctx          context.Context
	}{
		{
			name:     "GetLiveViews correctly translates from scriptmgrpb to cloudpb.",
			endpoint: "GetLiveViews",
			ctx:      CreateAPIUserTestContext(),
			smReq:    &scriptmgrpb.GetLiveViewsReq{},
			smResp: &scriptmgrpb.GetLiveViewsResp{
				LiveViews: []*scriptmgrpb.LiveViewMetadata{
					{
						ID:   utils.ProtoFromUUID(ID1),
						Name: "liveview1",
						Desc: "liveview1 desc",
					},
					{
						ID:   utils.ProtoFromUUID(ID2),
						Name: "liveview2",
						Desc: "liveview2 desc",
					},
				},
			},
			req: &cloudpb.GetLiveViewsReq{},
			expectedResp: &cloudpb.GetLiveViewsResp{
				LiveViews: []*cloudpb.LiveViewMetadata{
					{
						ID:   ID1.String(),
						Name: "liveview1",
						Desc: "liveview1 desc",
					},
					{
						ID:   ID2.String(),
						Name: "liveview2",
						Desc: "liveview2 desc",
					},
				},
			},
		},
		{
			name:     "GetLiveViewContents correctly translates between scriptmgr and cloudpb.",
			endpoint: "GetLiveViewContents",
			ctx:      CreateTestContext(),
			smReq: &scriptmgrpb.GetLiveViewContentsReq{
				LiveViewID: utils.ProtoFromUUID(ID1),
			},
			smResp: &scriptmgrpb.GetLiveViewContentsResp{
				Metadata: &scriptmgrpb.LiveViewMetadata{
					ID:   utils.ProtoFromUUID(ID1),
					Name: "liveview1",
					Desc: "liveview1 desc",
				},
				PxlContents: "liveview1 pxl",
				Vis:         testVis,
			},
			req: &cloudpb.GetLiveViewContentsReq{
				LiveViewID: ID1.String(),
			},
			expectedResp: &cloudpb.GetLiveViewContentsResp{
				Metadata: &cloudpb.LiveViewMetadata{
					ID:   ID1.String(),
					Name: "liveview1",
					Desc: "liveview1 desc",
				},
				PxlContents: "liveview1 pxl",
				Vis:         testVis,
			},
		},
		{
			name:     "GetScripts correctly translates between scriptmgr and cloudpb.",
			endpoint: "GetScripts",
			ctx:      CreateTestContext(),
			smReq:    &scriptmgrpb.GetScriptsReq{},
			smResp: &scriptmgrpb.GetScriptsResp{
				Scripts: []*scriptmgrpb.ScriptMetadata{
					{
						ID:          utils.ProtoFromUUID(ID1),
						Name:        "script1",
						Desc:        "script1 desc",
						HasLiveView: false,
					},
					{
						ID:          utils.ProtoFromUUID(ID2),
						Name:        "liveview1",
						Desc:        "liveview1 desc",
						HasLiveView: true,
					},
				},
			},
			req: &cloudpb.GetScriptsReq{},
			expectedResp: &cloudpb.GetScriptsResp{
				Scripts: []*cloudpb.ScriptMetadata{
					{
						ID:          ID1.String(),
						Name:        "script1",
						Desc:        "script1 desc",
						HasLiveView: false,
					},
					{
						ID:          ID2.String(),
						Name:        "liveview1",
						Desc:        "liveview1 desc",
						HasLiveView: true,
					},
				},
			},
		},
		{
			name:     "GetScriptContents correctly translates between scriptmgr and cloudpb.",
			endpoint: "GetScriptContents",
			ctx:      CreateTestContext(),
			smReq: &scriptmgrpb.GetScriptContentsReq{
				ScriptID: utils.ProtoFromUUID(ID1),
			},
			smResp: &scriptmgrpb.GetScriptContentsResp{
				Metadata: &scriptmgrpb.ScriptMetadata{
					ID:          utils.ProtoFromUUID(ID1),
					Name:        "Script1",
					Desc:        "Script1 desc",
					HasLiveView: false,
				},
				Contents: "Script1 pxl",
			},
			req: &cloudpb.GetScriptContentsReq{
				ScriptID: ID1.String(),
			},
			expectedResp: &cloudpb.GetScriptContentsResp{
				Metadata: &cloudpb.ScriptMetadata{
					ID:          ID1.String(),
					Name:        "Script1",
					Desc:        "Script1 desc",
					HasLiveView: false,
				},
				Contents: "Script1 pxl",
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			mockScriptMgr := mock_scriptmgr.NewMockScriptMgrServiceClient(ctrl)
			ctx := tc.ctx

			reflect.ValueOf(mockScriptMgr.EXPECT()).
				MethodByName(tc.endpoint).
				Call([]reflect.Value{
					reflect.ValueOf(gomock.Any()),
					reflect.ValueOf(tc.smReq),
				})[0].Interface().(*gomock.Call).
				Return(tc.smResp, nil)

			scriptMgrServer := &controllers.ScriptMgrServer{
				ScriptMgr: mockScriptMgr,
			}

			returnVals := reflect.ValueOf(scriptMgrServer).
				MethodByName(tc.endpoint).
				Call([]reflect.Value{
					reflect.ValueOf(ctx),
					reflect.ValueOf(tc.req),
				})
			assert.Nil(t, returnVals[1].Interface())
			resp := returnVals[0].Interface().(proto.Message)

			assert.Equal(t, tc.expectedResp, resp)
		})
	}
}
