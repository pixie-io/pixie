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

package main_test

import (
	"context"
	"fmt"
	"os"
	"testing"

	"github.com/gogo/protobuf/types"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/golang/mock/gomock"
	"github.com/jmoiron/sqlx"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"

	main "px.dev/pixie/src/cloud/plugin/load_db"
	"px.dev/pixie/src/cloud/plugin/pluginpb"
	mock_pluginpb "px.dev/pixie/src/cloud/plugin/pluginpb/mock"
	"px.dev/pixie/src/cloud/plugin/schema"
	"px.dev/pixie/src/shared/services/pgtest"
	"px.dev/pixie/src/utils"
)

var db *sqlx.DB

func TestMain(m *testing.M) {
	err := testMain(m)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Got error: %v\n", err)
		os.Exit(1)
	}
	os.Exit(0)
}

func testMain(m *testing.M) error {
	viper.Set("jwt_signing_key", "key0")
	s := bindata.Resource(schema.AssetNames(), schema.Asset)
	testDB, teardown, err := pgtest.SetupTestDB(s)
	if err != nil {
		return fmt.Errorf("failed to start test database: %w", err)
	}

	defer teardown()
	db = testDB

	if c := m.Run(); c != 0 {
		return fmt.Errorf("some tests failed with code: %d", c)
	}
	return nil
}

type enabledPlugin struct {
	pluginID string
	version  string
}

func loadTestData(db *sqlx.DB, pluginVersions map[string][]string, enabledPlugins map[string][]*enabledPlugin) {
	db.MustExec(`DELETE FROM plugin_retention_scripts`)
	db.MustExec(`DELETE FROM org_data_retention_plugins`)
	db.MustExec(`DELETE FROM data_retention_plugin_releases`)
	db.MustExec(`DELETE FROM plugin_releases`)

	insertRelease := `INSERT INTO plugin_releases(name, id, version, data_retention_enabled) VALUES ($1, $2, $3, $4)`
	insertRetentionRelease := `INSERT INTO data_retention_plugin_releases(plugin_id, version) VALUES ($1, $2)`

	for k, versions := range pluginVersions {
		for _, version := range versions {
			db.MustExec(insertRelease, k, k, version, true)
			db.MustExec(insertRetentionRelease, k, version)
		}
	}

	insertOrgRelease := `INSERT INTO org_data_retention_plugins(org_id, plugin_id, version) VALUES ($1, $2, $3)`
	for org, plugins := range enabledPlugins {
		for _, p := range plugins {
			db.MustExec(insertOrgRelease, org, p.pluginID, p.version)
		}
	}
}

func TestLoadDB_UpdatePlugins(t *testing.T) {
	loadTestData(db, map[string][]string{
		"test-plugin": {
			"0.0.1", "0.0.2", "0.0.3", "0.1.0", "0.1.1", "0.1.2", "1.0.0", "1.1.0",
		},
		"another-plugin": {
			"0.0.1", "0.0.2",
		},
	}, map[string][]*enabledPlugin{
		"223e4567-e89b-12d3-a456-426655440000": {
			{
				pluginID: "test-plugin",
				version:  "0.0.3",
			},
		},
		"223e4567-e89b-12d3-a456-426655440001": {
			{
				pluginID: "test-plugin",
				version:  "0.1.2",
			},
			{
				pluginID: "another-plugin",
				version:  "0.0.1",
			},
		},
		"223e4567-e89b-12d3-a456-426655440002": {
			{
				pluginID: "test-plugin",
				version:  "1.0.0",
			},
			{
				pluginID: "another-plugin",
				version:  "0.0.2",
			},
		},
	})

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockClient := mock_pluginpb.NewMockDataRetentionPluginServiceClient(ctrl)

	updateReqs := make([]*pluginpb.UpdateOrgRetentionPluginConfigRequest, 0)
	mockClient.EXPECT().UpdateOrgRetentionPluginConfig(gomock.Any(), gomock.Any()).DoAndReturn(func(ctx context.Context, req *pluginpb.UpdateOrgRetentionPluginConfigRequest) (*pluginpb.UpdateOrgRetentionPluginConfigResponse, error) {
		updateReqs = append(updateReqs, req)
		return &pluginpb.UpdateOrgRetentionPluginConfigResponse{}, nil
	}).AnyTimes()

	main.UpdatePlugins(db, mockClient)

	assert.ElementsMatch(t, []*pluginpb.UpdateOrgRetentionPluginConfigRequest{
		{
			OrgID:    utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
			Version:  &types.StringValue{Value: "0.1.2"},
			PluginID: "test-plugin",
		},
		{
			OrgID:    utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
			Version:  &types.StringValue{Value: "0.0.2"},
			PluginID: "another-plugin",
		},
		{
			OrgID:    utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
			Version:  &types.StringValue{Value: "1.1.0"},
			PluginID: "test-plugin",
		},
	}, updateReqs)
}
