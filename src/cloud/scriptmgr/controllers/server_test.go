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
	"encoding/json"
	"fmt"
	"testing"
	"time"

	"cloud.google.com/go/storage"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/jsonpb"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/vispb"
	"px.dev/pixie/src/cloud/scriptmgr/controllers"
	"px.dev/pixie/src/cloud/scriptmgr/scriptmgrpb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

const bundleBucket = "test-bucket"
const bundlePath = "bundle.json"

type scriptDef = map[string]string
type scriptsDef = map[string]scriptDef

var testLiveView = `{
 	"widgets": [{
		"func": {
			"name": "make_output",
			"args": [{
				"name": "start_time",
				"value": "-1m"
			}]
		},
		"displaySpec": {
			"@type": "types.px.dev/px.vispb.Table"
		}
	}]
}`

var testBundle = map[string]scriptsDef{
	"scripts": {
		"script1": scriptDef{
			"pxl":       "script1 pxl",
			"placement": "",
			"vis":       "",
			"ShortDoc":  "script1 desc",
			"LongDoc":   "",
		},
		"liveview1": {
			"pxl":       "liveview1 pxl",
			"vis":       testLiveView,
			"placement": "",
			"ShortDoc":  "liveview1 desc",
			"LongDoc":   "",
		},
		"script2": {
			"pxl":       "script2 pxl",
			"vis":       "",
			"placement": "",
			"ShortDoc":  "script2 desc",
			"LongDoc":   "",
		},
	},
}

func mustSetupFakeBucket(t *testing.T, testBundle map[string]scriptsDef) stiface.Client {
	bundleJSON, err := json.Marshal(testBundle)
	require.NoError(t, err)

	return testingutils.NewMockGCSClient(map[string]*testingutils.MockGCSBucket{
		bundleBucket: testingutils.NewMockGCSBucket(
			map[string]*testingutils.MockGCSObject{
				bundlePath: testingutils.NewMockGCSObject(
					bundleJSON,
					&storage.ObjectAttrs{
						Updated: time.Now(),
					},
				),
			},
			nil,
		),
	})
}

func TestScriptMgr_GetLiveViews(t *testing.T) {
	testCases := []struct {
		name         string
		expectedResp *scriptmgrpb.GetLiveViewsResp
		expectErr    bool
	}{
		{
			name: "Empty live view request returns all live views.",
			expectedResp: &scriptmgrpb.GetLiveViewsResp{
				LiveViews: []*scriptmgrpb.LiveViewMetadata{
					{
						ID:   nil,
						Name: "liveview1",
						Desc: "liveview1 desc",
					},
				},
			},
			expectErr: false,
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			c := mustSetupFakeBucket(t, testBundle)
			s := controllers.NewServer(bundleBucket, bundlePath, c)
			ctx := context.Background()

			req := &scriptmgrpb.GetLiveViewsReq{}
			resp, err := s.GetLiveViews(ctx, req)
			if tc.expectErr {
				require.NotNil(t, err)
			} else {
				require.NoError(t, err)
				// Ignore UUID in equality check.
				for _, liveView := range resp.LiveViews {
					liveView.ID = nil
				}
				assert.ElementsMatch(t, tc.expectedResp.LiveViews, resp.LiveViews)
			}
		})
	}
}

func TestScriptMgr_GetLiveViewContents(t *testing.T) {
	testCases := []struct {
		name         string
		liveViewName string
		expectErr    bool
		errCode      codes.Code
	}{
		{
			name:         "Valid UUID should return live view.",
			liveViewName: "liveview1",
			expectErr:    false,
			errCode:      codes.OK,
		},
		{
			name:         "UUID not in bundle should return error.",
			liveViewName: "not-a-real-live-view",
			expectErr:    true,
			errCode:      codes.InvalidArgument,
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			c := mustSetupFakeBucket(t, testBundle)
			s := controllers.NewServer(bundleBucket, bundlePath, c)
			ctx := context.Background()

			id := uuid.NewV5(s.SeedUUID, tc.liveViewName)
			req := &scriptmgrpb.GetLiveViewContentsReq{
				LiveViewID: utils.ProtoFromUUID(id),
			}

			resp, err := s.GetLiveViewContents(ctx, req)
			if tc.expectErr {
				require.NotNil(t, err)
				status, ok := status.FromError(err)
				require.True(t, ok)
				assert.Equal(t, tc.errCode, status.Code())
				return
			}

			var vis vispb.Vis
			err = jsonpb.UnmarshalString(testBundle["scripts"][tc.liveViewName]["vis"], &vis)
			require.NoError(t, err)
			// Make sure a future bug in the test doesn't accidentally expect the "0 value" for Vis.
			if testBundle["scripts"][tc.liveViewName]["vis"] != "" {
				require.True(t, len(vis.Widgets) > 0)
			}

			expectedResp := &scriptmgrpb.GetLiveViewContentsResp{
				Metadata: &scriptmgrpb.LiveViewMetadata{
					ID:   utils.ProtoFromUUID(id),
					Name: tc.liveViewName,
					Desc: fmt.Sprintf("%s desc", tc.liveViewName),
				},
				PxlContents: fmt.Sprintf("%s pxl", tc.liveViewName),
				Vis:         &vis,
			}

			require.NoError(t, err)
			assert.Equal(t, expectedResp, resp)
		})
	}
}

func TestScriptMgr_GetScripts(t *testing.T) {
	testCases := []struct {
		name         string
		expectedResp *scriptmgrpb.GetScriptsResp
		expectErr    bool
	}{
		{
			name: "Empty request returns all scripts, including scripts with live views.",
			expectedResp: &scriptmgrpb.GetScriptsResp{
				Scripts: []*scriptmgrpb.ScriptMetadata{
					{
						ID:          nil,
						Name:        "script1",
						Desc:        "script1 desc",
						HasLiveView: false,
					},
					{
						ID:          nil,
						Name:        "script2",
						Desc:        "script2 desc",
						HasLiveView: false,
					},
					{
						ID:          nil,
						Name:        "liveview1",
						Desc:        "liveview1 desc",
						HasLiveView: true,
					},
				},
			},
			expectErr: false,
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			c := mustSetupFakeBucket(t, testBundle)
			s := controllers.NewServer(bundleBucket, bundlePath, c)
			ctx := context.Background()

			req := &scriptmgrpb.GetScriptsReq{}
			resp, err := s.GetScripts(ctx, req)
			if tc.expectErr {
				require.NotNil(t, err)
			} else {
				require.NoError(t, err)
				// Ignore UUID in equality check.
				for _, script := range resp.Scripts {
					script.ID = nil
				}
				assert.ElementsMatch(t, tc.expectedResp.Scripts, resp.Scripts)
			}
		})
	}
}

func TestScriptMgr_GetScriptContents(t *testing.T) {
	testCases := []struct {
		name       string
		scriptName string
		expectErr  bool
		errCode    codes.Code
	}{
		{
			name:       "Valid UUID should return script.",
			scriptName: "script2",
			expectErr:  false,
			errCode:    codes.OK,
		},
		{
			name:       "UUID not in bundle returns error.",
			scriptName: "not-a-real-script",
			expectErr:  true,
			errCode:    codes.InvalidArgument,
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			c := mustSetupFakeBucket(t, testBundle)
			s := controllers.NewServer(bundleBucket, bundlePath, c)
			ctx := context.Background()
			id := uuid.NewV5(s.SeedUUID, tc.scriptName)
			req := &scriptmgrpb.GetScriptContentsReq{
				ScriptID: utils.ProtoFromUUID(id),
			}
			expectedResp := &scriptmgrpb.GetScriptContentsResp{
				Metadata: &scriptmgrpb.ScriptMetadata{
					ID:          utils.ProtoFromUUID(id),
					Name:        tc.scriptName,
					Desc:        fmt.Sprintf("%s desc", tc.scriptName),
					HasLiveView: false,
				},
				Contents: fmt.Sprintf("%s pxl", tc.scriptName),
			}
			resp, err := s.GetScriptContents(ctx, req)
			if tc.expectErr {
				require.NotNil(t, err)
				status, ok := status.FromError(err)
				require.True(t, ok)
				assert.Equal(t, tc.errCode, status.Code())
			} else {
				require.NoError(t, err)
				assert.Equal(t, expectedResp, resp)
			}
		})
	}
}
