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

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/cloud/plugin/pluginpb"
	"px.dev/pixie/src/utils"
)

func TestGetPlugins(t *testing.T) {
	tests := []struct {
		name                string
		ctx                 context.Context
		allPlugins          []*pluginpb.Plugin
		orgRetentionPlugins []*pluginpb.GetRetentionPluginsForOrgResponse_PluginState
		expectedPlugins     []*cloudpb.Plugin
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
			allPlugins: []*pluginpb.Plugin{
				{
					Name:             "Test Plugin",
					ID:               "test-plugin",
					Description:      "Here is a plugin that is used for this test",
					Logo:             "",
					LatestVersion:    "2.0.0",
					RetentionEnabled: true,
				},
				{
					Name:             "Another Plugin",
					ID:               "another-plugin",
					Description:      "Here is a another plugin that is used for this test",
					Logo:             "",
					LatestVersion:    "3.0.0",
					RetentionEnabled: true,
				},
			},
			orgRetentionPlugins: []*pluginpb.GetRetentionPluginsForOrgResponse_PluginState{
				{
					Plugin: &pluginpb.Plugin{
						Name:             "Another Plugin",
						ID:               "another-plugin",
						Description:      "Here is a another plugin that is used for this test",
						Logo:             "",
						LatestVersion:    "3.0.0",
						RetentionEnabled: true,
					},
					EnabledVersion: "2.0.0",
				},
			},
			expectedPlugins: []*cloudpb.Plugin{
				{
					Name:               "Test Plugin",
					Id:                 "test-plugin",
					Description:        "Here is a plugin that is used for this test",
					Logo:               "",
					LatestVersion:      "2.0.0",
					RetentionSupported: true,
					RetentionEnabled:   false,
					EnabledVersion:     "",
				},
				{
					Name:               "Another Plugin",
					Id:                 "another-plugin",
					Description:        "Here is a another plugin that is used for this test",
					Logo:               "",
					LatestVersion:      "3.0.0",
					RetentionSupported: true,
					RetentionEnabled:   true,
					EnabledVersion:     "2.0.0",
				},
			},
		},
		{
			name: "API user",
			ctx:  CreateAPIUserTestContext(),
			allPlugins: []*pluginpb.Plugin{
				{
					Name:             "Test Plugin",
					ID:               "test-plugin",
					Description:      "Here is a plugin that is used for this test",
					Logo:             "",
					LatestVersion:    "2.0.0",
					RetentionEnabled: true,
				},
				{
					Name:             "Another Plugin",
					ID:               "another-plugin",
					Description:      "Here is a another plugin that is used for this test",
					Logo:             "",
					LatestVersion:    "3.0.0",
					RetentionEnabled: true,
				},
			},
			orgRetentionPlugins: []*pluginpb.GetRetentionPluginsForOrgResponse_PluginState{
				{
					Plugin: &pluginpb.Plugin{
						Name:             "Another Plugin",
						ID:               "another-plugin",
						Description:      "Here is a another plugin that is used for this test",
						Logo:             "",
						LatestVersion:    "3.0.0",
						RetentionEnabled: true,
					},
					EnabledVersion: "2.0.0",
				},
			},
			expectedPlugins: []*cloudpb.Plugin{
				{
					Name:               "Test Plugin",
					Id:                 "test-plugin",
					Description:        "Here is a plugin that is used for this test",
					Logo:               "",
					LatestVersion:      "2.0.0",
					RetentionSupported: true,
					RetentionEnabled:   false,
					EnabledVersion:     "",
				},
				{
					Name:               "Another Plugin",
					Id:                 "another-plugin",
					Description:        "Here is a another plugin that is used for this test",
					Logo:               "",
					LatestVersion:      "3.0.0",
					RetentionSupported: true,
					RetentionEnabled:   true,
					EnabledVersion:     "2.0.0",
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

			mockReq1 := &pluginpb.GetPluginsRequest{
				Kind: pluginpb.PLUGIN_KIND_RETENTION,
			}

			mockClients.MockPlugin.EXPECT().GetPlugins(gomock.Any(), mockReq1).
				Return(&pluginpb.GetPluginsResponse{
					Plugins: test.allPlugins,
				}, nil)

			mockReq2 := &pluginpb.GetRetentionPluginsForOrgRequest{
				OrgID: orgID,
			}

			mockClients.MockDataRetentionPlugin.EXPECT().GetRetentionPluginsForOrg(gomock.Any(), mockReq2).
				Return(&pluginpb.GetRetentionPluginsForOrgResponse{
					Plugins: test.orgRetentionPlugins,
				}, nil)

			pServer := &controllers.PluginServiceServer{mockClients.MockPlugin, mockClients.MockDataRetentionPlugin}

			resp, err := pServer.GetPlugins(ctx, &cloudpb.GetPluginsRequest{
				Kind: cloudpb.PK_RETENTION,
			})

			require.NoError(t, err)
			assert.Equal(t, test.expectedPlugins, resp.Plugins)
		})
	}
}

func TestGetOrgRetentionPluginConfig(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	mockReq := &pluginpb.GetOrgRetentionPluginConfigRequest{
		OrgID:    orgID,
		PluginID: "test-plugin",
	}

	mockClients.MockDataRetentionPlugin.EXPECT().GetOrgRetentionPluginConfig(gomock.Any(), mockReq).
		Return(&pluginpb.GetOrgRetentionPluginConfigResponse{
			Configurations: map[string]string{
				"API_KEY": "test-api-key",
			},
			CustomExportUrl: "https://localhost:8080",
			InsecureTLS:     true,
		}, nil)

	pServer := &controllers.PluginServiceServer{mockClients.MockPlugin, mockClients.MockDataRetentionPlugin}

	resp, err := pServer.GetOrgRetentionPluginConfig(ctx, &cloudpb.GetOrgRetentionPluginConfigRequest{
		PluginId: "test-plugin",
	})

	require.NoError(t, err)
	assert.Equal(t, &cloudpb.GetOrgRetentionPluginConfigResponse{
		Configs: map[string]string{
			"API_KEY": "test-api-key",
		},
		CustomExportUrl: "https://localhost:8080",
		InsecureTLS:     true,
	}, resp)
}

func TestGetRetentionPluginInfo(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockReq := &pluginpb.GetRetentionPluginConfigRequest{
		Version: "2.0.0",
		ID:      "test-plugin",
	}

	mockClients.MockPlugin.EXPECT().GetRetentionPluginConfig(gomock.Any(), mockReq).
		Return(&pluginpb.GetRetentionPluginConfigResponse{
			Configurations: map[string]string{
				"API_KEY": "This is the API key used in the product.",
			},
			AllowCustomExportURL: true,
			AllowInsecureTLS:     true,
			DefaultExportURL:     "https://test.com",
		}, nil)

	pServer := &controllers.PluginServiceServer{mockClients.MockPlugin, mockClients.MockDataRetentionPlugin}

	resp, err := pServer.GetRetentionPluginInfo(ctx, &cloudpb.GetRetentionPluginInfoRequest{
		PluginId: "test-plugin",
		Version:  "2.0.0",
	})

	require.NoError(t, err)
	assert.Equal(t, &cloudpb.GetRetentionPluginInfoResponse{
		Configs: map[string]string{
			"API_KEY": "This is the API key used in the product.",
		},
		AllowCustomExportURL: true,
		AllowInsecureTLS:     true,
		DefaultExportURL:     "https://test.com",
	}, resp)
}

func TestUpdateRetentionPluginConfig(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	mockReq := &pluginpb.UpdateOrgRetentionPluginConfigRequest{
		OrgID:    orgID,
		PluginID: "test-plugin",
		Configurations: map[string]string{
			"API_KEY": "test-api-key",
		},
		Enabled:         &types.BoolValue{Value: true},
		Version:         &types.StringValue{Value: "2.0.0"},
		CustomExportUrl: &types.StringValue{Value: "https://localhost:8080"},
		InsecureTLS:     &types.BoolValue{Value: true},
		DisablePresets:  &types.BoolValue{Value: true},
	}

	mockClients.MockDataRetentionPlugin.EXPECT().UpdateOrgRetentionPluginConfig(gomock.Any(), mockReq).
		Return(&pluginpb.UpdateOrgRetentionPluginConfigResponse{}, nil)

	pServer := &controllers.PluginServiceServer{mockClients.MockPlugin, mockClients.MockDataRetentionPlugin}

	resp, err := pServer.UpdateRetentionPluginConfig(ctx, &cloudpb.UpdateRetentionPluginConfigRequest{
		PluginId: "test-plugin",
		Configs: map[string]string{
			"API_KEY": "test-api-key",
		},
		Enabled:         &types.BoolValue{Value: true},
		Version:         &types.StringValue{Value: "2.0.0"},
		CustomExportUrl: &types.StringValue{Value: "https://localhost:8080"},
		InsecureTLS:     &types.BoolValue{Value: true},
		DisablePresets:  &types.BoolValue{Value: true},
	})

	require.NoError(t, err)
	assert.Equal(t, &cloudpb.UpdateRetentionPluginConfigResponse{}, resp)
}

func TestGetRetentionScripts(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	mockReq := &pluginpb.GetRetentionScriptsRequest{
		OrgID: orgID,
	}

	mockClients.MockDataRetentionPlugin.EXPECT().GetRetentionScripts(gomock.Any(), mockReq).
		Return(&pluginpb.GetRetentionScriptsResponse{
			Scripts: []*pluginpb.RetentionScript{
				{
					ScriptID:    utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8"),
					ScriptName:  "Test Script",
					Description: "This is a script",
					FrequencyS:  5,
					ClusterIDs: []*uuidpb.UUID{
						utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
						utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c1"),
					},
					PluginId: "test-plugin",
					Enabled:  true,
					IsPreset: false,
				},
				{
					ScriptID:    utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c1"),
					ScriptName:  "Another Script",
					Description: "This is another script",
					FrequencyS:  20,
					ClusterIDs: []*uuidpb.UUID{
						utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
					},
					PluginId: "test-plugin-2",
					Enabled:  false,
					IsPreset: true,
				},
			},
		}, nil)

	pServer := &controllers.PluginServiceServer{mockClients.MockPlugin, mockClients.MockDataRetentionPlugin}

	resp, err := pServer.GetRetentionScripts(ctx, &cloudpb.GetRetentionScriptsRequest{})

	require.NoError(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, &cloudpb.GetRetentionScriptsResponse{
		Scripts: []*cloudpb.RetentionScript{
			{
				ScriptID:    utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				ScriptName:  "Test Script",
				Description: "This is a script",
				FrequencyS:  5,
				ClusterIDs: []*uuidpb.UUID{
					utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
					utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c1"),
				},
				PluginId: "test-plugin",
				Enabled:  true,
				IsPreset: false,
			},
			{
				ScriptID:    utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c1"),
				ScriptName:  "Another Script",
				Description: "This is another script",
				FrequencyS:  20,
				ClusterIDs: []*uuidpb.UUID{
					utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				},
				PluginId: "test-plugin-2",
				Enabled:  false,
				IsPreset: true,
			},
		},
	}, resp)
}

func TestGetRetentionScript(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	scriptID := utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8")

	mockReq := &pluginpb.GetRetentionScriptRequest{
		OrgID:    orgID,
		ScriptID: scriptID,
	}

	mockClients.MockDataRetentionPlugin.EXPECT().GetRetentionScript(gomock.Any(), mockReq).
		Return(&pluginpb.GetRetentionScriptResponse{
			Script: &pluginpb.DetailedRetentionScript{
				Script: &pluginpb.RetentionScript{
					ScriptID:    utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8"),
					ScriptName:  "Test Script",
					Description: "This is a script",
					FrequencyS:  5,
					ClusterIDs: []*uuidpb.UUID{
						utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
						utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c1"),
					},
					PluginId: "test-plugin",
					Enabled:  true,
					IsPreset: false,
				},
				Contents:  "px.display()",
				ExportURL: "https://localhost:8001",
			},
		}, nil)

	pServer := &controllers.PluginServiceServer{mockClients.MockPlugin, mockClients.MockDataRetentionPlugin}

	resp, err := pServer.GetRetentionScript(ctx, &cloudpb.GetRetentionScriptRequest{
		ID: scriptID,
	})

	require.NoError(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, &cloudpb.GetRetentionScriptResponse{
		Script: &cloudpb.RetentionScript{
			ScriptID:    utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			ScriptName:  "Test Script",
			Description: "This is a script",
			FrequencyS:  5,
			ClusterIDs: []*uuidpb.UUID{
				utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c1"),
			},
			PluginId: "test-plugin",
			Enabled:  true,
			IsPreset: false,
		},
		Contents:  "px.display()",
		ExportURL: "https://localhost:8001",
	}, resp)
}

func TestUpdateRetentionScript(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	scriptID := utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8")

	mockReq := &pluginpb.UpdateRetentionScriptRequest{
		ScriptID:    scriptID,
		ScriptName:  &types.StringValue{Value: "abcd"},
		Description: &types.StringValue{Value: "desc"},
		FrequencyS:  &types.Int64Value{Value: 10},
		ClusterIDs: []*uuidpb.UUID{
			utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c1"),
		},
		ExportUrl: &types.StringValue{Value: "newurl"},
		Contents:  &types.StringValue{Value: "new contents"},
		Enabled:   &types.BoolValue{Value: false},
	}

	mockClients.MockDataRetentionPlugin.EXPECT().UpdateRetentionScript(gomock.Any(), mockReq).
		Return(&pluginpb.UpdateRetentionScriptResponse{}, nil)

	pServer := &controllers.PluginServiceServer{mockClients.MockPlugin, mockClients.MockDataRetentionPlugin}

	resp, err := pServer.UpdateRetentionScript(ctx, &cloudpb.UpdateRetentionScriptRequest{
		ID:          scriptID,
		ScriptName:  &types.StringValue{Value: "abcd"},
		Description: &types.StringValue{Value: "desc"},
		FrequencyS:  &types.Int64Value{Value: 10},
		ClusterIDs: []*uuidpb.UUID{
			utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c1"),
		},
		ExportUrl: &types.StringValue{Value: "newurl"},
		Contents:  &types.StringValue{Value: "new contents"},
		Enabled:   &types.BoolValue{Value: false},
	})

	require.NoError(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, &cloudpb.UpdateRetentionScriptResponse{}, resp)
}

func TestCreateRetentionScript(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	scriptID := utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8")

	mockReq := &pluginpb.CreateRetentionScriptRequest{
		OrgID: orgID,
		Script: &pluginpb.DetailedRetentionScript{
			Script: &pluginpb.RetentionScript{
				ScriptName:  "Test Script",
				Description: "This is a script",
				FrequencyS:  5,
				ClusterIDs: []*uuidpb.UUID{
					utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
					utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c1"),
				},
				PluginId: "test-plugin",
				Enabled:  true,
				IsPreset: false,
			},
			Contents:  "px.display()",
			ExportURL: "https://localhost:8001",
		},
	}

	mockClients.MockDataRetentionPlugin.EXPECT().CreateRetentionScript(gomock.Any(), mockReq).
		Return(&pluginpb.CreateRetentionScriptResponse{ID: scriptID}, nil)

	pServer := &controllers.PluginServiceServer{mockClients.MockPlugin, mockClients.MockDataRetentionPlugin}

	resp, err := pServer.CreateRetentionScript(ctx, &cloudpb.CreateRetentionScriptRequest{
		ScriptName:  "Test Script",
		Description: "This is a script",
		FrequencyS:  5,
		ClusterIDs: []*uuidpb.UUID{
			utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c1"),
		},
		PluginId:  "test-plugin",
		Contents:  "px.display()",
		ExportUrl: "https://localhost:8001",
	})

	require.NoError(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, &cloudpb.CreateRetentionScriptResponse{
		ID: scriptID,
	}, resp)
}

func TestDeleteRetentionScript(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	scriptID := utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8")

	mockReq := &pluginpb.DeleteRetentionScriptRequest{
		ID:    scriptID,
		OrgID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
	}

	mockClients.MockDataRetentionPlugin.EXPECT().DeleteRetentionScript(gomock.Any(), mockReq).
		Return(&pluginpb.DeleteRetentionScriptResponse{}, nil)

	pServer := &controllers.PluginServiceServer{mockClients.MockPlugin, mockClients.MockDataRetentionPlugin}

	resp, err := pServer.DeleteRetentionScript(ctx, &cloudpb.DeleteRetentionScriptRequest{
		ID: scriptID,
	})

	require.NoError(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, &cloudpb.DeleteRetentionScriptResponse{}, resp)
}
