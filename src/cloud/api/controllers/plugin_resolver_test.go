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
	"testing"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	gqltestutils "px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/utils"
)

func TestPluginResolver_Plugins(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().GetPlugins(gomock.Any(), &cloudpb.GetPluginsRequest{
		Kind: cloudpb.PK_RETENTION,
	}).Return(&cloudpb.GetPluginsResponse{
		Plugins: []*cloudpb.Plugin{
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
				Logo:               "<svg></svg>",
				LatestVersion:      "3.0.0",
				RetentionSupported: true,
				RetentionEnabled:   true,
				EnabledVersion:     "2.0.0",
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
					plugins(kind: PK_RETENTION) {
						name
						id
						description
						logo
						latestVersion
						supportsRetention
						retentionEnabled
						enabledVersion
					}
				}
			`,
			ExpectedResult: `
				{
					"plugins": [
						{

							"name": "Test Plugin",
							"id": "test-plugin",
							"description": "Here is a plugin that is used for this test",
							"logo": null,
							"latestVersion": "2.0.0",
							"supportsRetention": true,
							"retentionEnabled": false,
							"enabledVersion": null
						},
						{

							"name": "Another Plugin",
							"id": "another-plugin",
							"description": "Here is a another plugin that is used for this test",
							"logo": "<svg></svg>",
							"latestVersion": "3.0.0",
							"supportsRetention": true,
							"retentionEnabled": true,
							"enabledVersion": "2.0.0"
						}
					]
				}
			`,
		},
	})
}

func TestPluginResolver_OrgRetentionPluginConfig(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().GetOrgRetentionPluginConfig(gomock.Any(), &cloudpb.GetOrgRetentionPluginConfigRequest{
		PluginId: "test-plugin",
	}).Return(&cloudpb.GetOrgRetentionPluginConfigResponse{
		Configs: map[string]string{
			"API_KEY": "test-api-key",
		},
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					orgRetentionPluginConfig(id: "test-plugin") {
						name
						value
					}
				}
			`,
			ExpectedResult: `
				{
					"orgRetentionPluginConfig": [
						{

							"name": "API_KEY",
							"value": "test-api-key"
						}
					]
				}
			`,
		},
	})
}

func TestPluginResolver_RetentionPluginConfig(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().GetOrgRetentionPluginConfig(gomock.Any(), &cloudpb.GetOrgRetentionPluginConfigRequest{
		PluginId: "test-plugin",
	}).Return(&cloudpb.GetOrgRetentionPluginConfigResponse{
		Configs: map[string]string{
			"API_KEY": "test-api-key",
		},
		CustomExportUrl: "https://localhost:8080",
		InsecureTLS:     true,
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					retentionPluginConfig(id: "test-plugin") {
						configs {
							name
							value
						}
						customExportURL
						insecureTLS
					}
				}
			`,
			ExpectedResult: `
				{
					"retentionPluginConfig": {
						"configs": [
							{

								"name": "API_KEY",
								"value": "test-api-key"
							}
						],
						"customExportURL": "https://localhost:8080",
						"insecureTLS": true
					}
				}
			`,
		},
	})
}

func TestPluginResolver_RetentionPluginInfo(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().GetRetentionPluginInfo(gomock.Any(), &cloudpb.GetRetentionPluginInfoRequest{
		PluginId: "test-plugin",
		Version:  "2.0.0",
	}).Return(&cloudpb.GetRetentionPluginInfoResponse{
		Configs: map[string]string{
			"API_KEY": "This is an API key used in the product.",
		},
		AllowCustomExportURL: true,
		AllowInsecureTLS:     true,
		DefaultExportURL:     "https://localhost:8080",
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					retentionPluginInfo(id: "test-plugin", pluginVersion: "2.0.0") {
						configs {
							name
							description
						}
						allowCustomExportURL
						allowInsecureTLS
						defaultExportURL
					}
				}
			`,
			ExpectedResult: `
				{
					"retentionPluginInfo": {
						"configs": [
							{
								"name": "API_KEY",
								"description": "This is an API key used in the product."
							}
						],
						"allowCustomExportURL": true,
						"allowInsecureTLS": true,
						"defaultExportURL": "https://localhost:8080"
					}
				}
			`,
		},
	})
}

func TestPluginResolver_UpdateRetentionPluginConfig(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().UpdateRetentionPluginConfig(gomock.Any(), &cloudpb.UpdateRetentionPluginConfigRequest{
		Configs: map[string]string{
			"API_KEY": "test-api-key",
		},
		PluginId: "test-plugin",
		Enabled: &types.BoolValue{
			Value: true,
		},
		Version: &types.StringValue{
			Value: "2.0.0",
		},
		CustomExportUrl: &types.StringValue{
			Value: "https://localhost:8080",
		},
		InsecureTLS: &types.BoolValue{
			Value: false,
		},
	}).Return(&cloudpb.UpdateRetentionPluginConfigResponse{}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					UpdateRetentionPluginConfig(id: "test-plugin", enabled: true, enabledVersion: "2.0.0", configs: {
						configs: [
							{
								name: "API_KEY",
								value: "test-api-key"
							}
						],
						customExportURL: "https://localhost:8080",
						insecureTLS: false
					})
				}
			`,
			ExpectedResult: `
				{
					"UpdateRetentionPluginConfig": true
				}
			`,
		},
	})
}

func TestPluginResolver_RetentionScripts(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().GetRetentionScripts(gomock.Any(), &cloudpb.GetRetentionScriptsRequest{}).Return(&cloudpb.GetRetentionScriptsResponse{
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
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					retentionScripts {
						id
						name
						description
						frequencyS
						enabled
						clusters
						pluginID
						isPreset
					}
				}
			`,
			ExpectedResult: `
				{
					"retentionScripts": [
						{
							"id": "1ba7b810-9dad-11d1-80b4-00c04fd430c8",
							"name": "Test Script",
							"description": "This is a script",
							"frequencyS": 5,
							"enabled": true,
							"clusters": [
								"2ba7b810-9dad-11d1-80b4-00c04fd430c8",
								"2ba7b810-9dad-11d1-80b4-00c04fd430c1"
							],
							"pluginID": "test-plugin",
							"isPreset": false
						},
						{
							"id": "1ba7b810-9dad-11d1-80b4-00c04fd430c1",
							"name": "Another Script",
							"description": "This is another script",
							"frequencyS": 20,
							"enabled": false,
							"clusters": [
								"2ba7b810-9dad-11d1-80b4-00c04fd430c8"
							],
							"pluginID": "test-plugin-2",
							"isPreset": true
						}
					]
				}
			`,
		},
	})
}

func TestPluginResolver_RetentionScript(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().GetRetentionScript(gomock.Any(), &cloudpb.GetRetentionScriptRequest{ID: utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8")}).Return(&cloudpb.GetRetentionScriptResponse{
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
		ExportURL: "https://localhost:8080",
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					retentionScript(id: "1ba7b810-9dad-11d1-80b4-00c04fd430c8") {
						id
						name
						description
						frequencyS
						enabled
						clusters
						pluginID
						isPreset
						contents
						customExportURL
					}
				}
			`,
			ExpectedResult: `
				{
					"retentionScript":
						{
							"id": "1ba7b810-9dad-11d1-80b4-00c04fd430c8",
							"name": "Test Script",
							"description": "This is a script",
							"frequencyS": 5,
							"enabled": true,
							"clusters": [
								"2ba7b810-9dad-11d1-80b4-00c04fd430c8",
								"2ba7b810-9dad-11d1-80b4-00c04fd430c1"
							],
							"pluginID": "test-plugin",
							"isPreset": false,
							"contents": "px.display()",
							"customExportURL": "https://localhost:8080"
						}

				}
			`,
		},
	})
}

func TestPluginResolver_UpdateRetentionScript(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().UpdateRetentionScript(gomock.Any(), &cloudpb.UpdateRetentionScriptRequest{
		ID:          utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		ScriptName:  &types.StringValue{Value: "Test script"},
		Description: &types.StringValue{Value: "This is a script"},
		FrequencyS:  &types.Int64Value{Value: 5},
		ExportUrl:   &types.StringValue{Value: "https://localhost:8080"},
		ClusterIDs: []*uuidpb.UUID{
			utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c1"),
		},
	}).Return(&cloudpb.UpdateRetentionScriptResponse{}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					UpdateRetentionScript(id: "1ba7b810-9dad-11d1-80b4-00c04fd430c8", script: {
						name: "Test script",
						description: "This is a script",
						frequencyS: 5,
						clusters: [
							"2ba7b810-9dad-11d1-80b4-00c04fd430c8",
							"2ba7b810-9dad-11d1-80b4-00c04fd430c1"
						],
						customExportURL: "https://localhost:8080"
					}) {}
				}
			`,
			ExpectedResult: `
				{
					"UpdateRetentionScript": true
				}
			`,
		},
	})
}

func TestPluginResolver_CreateRetentionScript(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().CreateRetentionScript(gomock.Any(), &cloudpb.CreateRetentionScriptRequest{
		ScriptName:  "Test script",
		Description: "This is a script",
		FrequencyS:  5,
		Contents:    "px.display()",
		ExportUrl:   "localhost:8080",
		ClusterIDs: []*uuidpb.UUID{
			utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			utils.ProtoFromUUIDStrOrNil("2ba7b810-9dad-11d1-80b4-00c04fd430c1"),
		},
		PluginId: "test-plugin",
	}).Return(&cloudpb.CreateRetentionScriptResponse{
		ID: utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8"),
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					CreateRetentionScript(script: {
						name: "Test script",
						description: "This is a script",
						frequencyS: 5,
						contents: "px.display()",
						customExportURL: "localhost:8080",
						pluginID: "test-plugin"
						clusters: [
							"2ba7b810-9dad-11d1-80b4-00c04fd430c8",
							"2ba7b810-9dad-11d1-80b4-00c04fd430c1"
						]
					}) {}
				}
			`,
			ExpectedResult: `
				{
					"CreateRetentionScript": "1ba7b810-9dad-11d1-80b4-00c04fd430c8"
				}
			`,
		},
	})
}

func TestPluginResolver_DeleteRetentionScript(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().DeleteRetentionScript(gomock.Any(), &cloudpb.DeleteRetentionScriptRequest{
		ID: utils.ProtoFromUUIDStrOrNil("1ba7b810-9dad-11d1-80b4-00c04fd430c8"),
	}).Return(&cloudpb.DeleteRetentionScriptResponse{}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					DeleteRetentionScript(id: "1ba7b810-9dad-11d1-80b4-00c04fd430c8") {}
				}
			`,
			ExpectedResult: `
				{
					"DeleteRetentionScript": true
				}
			`,
		},
	})
}
