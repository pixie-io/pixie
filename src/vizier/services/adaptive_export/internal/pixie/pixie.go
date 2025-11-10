// Copyright 2018- The Pixie Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

package pixie

import (
	"context"
	"crypto/tls"
	"fmt"
	"strings"

	"github.com/gogo/protobuf/types"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/metadata"
	"px.dev/pixie/src/api/go/pxapi/utils"
	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"

	"px.dev/pixie/src/vizier/services/adaptive_export/internal/script"
)

const (
	clickhousePluginId = "clickhouse"
	exportUrlConfig    = "exportURL"
)

type Client struct {
	cloudAddr string
	ctx       context.Context

	grpcConn     *grpc.ClientConn
	pluginClient cloudpb.PluginServiceClient
}

func NewClient(ctx context.Context, apiKey string, cloudAddr string) (*Client, error) {
	if apiKey == "" {
		fmt.Println("WARNING: API key is empty!")
	}

	c := &Client{
		cloudAddr: cloudAddr,
		ctx:       metadata.AppendToOutgoingContext(ctx, "pixie-api-key", apiKey),
	}

	if err := c.init(); err != nil {
		return nil, err
	}

	return c, nil
}

func (c *Client) init() error {
	isInternal := strings.ContainsAny(c.cloudAddr, "cluster.local")

	tlsConfig := &tls.Config{InsecureSkipVerify: isInternal}
	creds := credentials.NewTLS(tlsConfig)

	conn, err := grpc.Dial(c.cloudAddr, grpc.WithTransportCredentials(creds))
	if err != nil {
		return err
	}

	c.grpcConn = conn
	c.pluginClient = cloudpb.NewPluginServiceClient(conn)
	return nil
}

func (c *Client) GetClickHousePlugin() (*cloudpb.Plugin, error) {
	req := &cloudpb.GetPluginsRequest{
		Kind: cloudpb.PK_RETENTION,
	}
	resp, err := c.pluginClient.GetPlugins(c.ctx, req)
	if err != nil {
		return nil, err
	}
	for _, plugin := range resp.Plugins {
		if plugin.Id == clickhousePluginId {
			return plugin, nil
		}
	}
	return nil, fmt.Errorf("the %s plugin could not be found", clickhousePluginId)
}

type ClickHousePluginConfig struct {
	ExportUrl string
}

func (c *Client) GetClickHousePluginConfig() (*ClickHousePluginConfig, error) {
	req := &cloudpb.GetOrgRetentionPluginConfigRequest{
		PluginId: clickhousePluginId,
	}
	resp, err := c.pluginClient.GetOrgRetentionPluginConfig(c.ctx, req)
	if err != nil {
		return nil, err
	}
	exportUrl := resp.CustomExportUrl
	if exportUrl == "" {
		exportUrl, err = c.getDefaultClickHouseExportUrl()
		if err != nil {
			return nil, err
		}
	}
	return &ClickHousePluginConfig{
		ExportUrl: exportUrl,
	}, nil
}

func (c *Client) getDefaultClickHouseExportUrl() (string, error) {
	req := &cloudpb.GetRetentionPluginInfoRequest{
		PluginId: clickhousePluginId,
	}
	info, err := c.pluginClient.GetRetentionPluginInfo(c.ctx, req)
	if err != nil {
		return "", err
	}
	return info.DefaultExportURL, nil
}

func (c *Client) EnableClickHousePlugin(config *ClickHousePluginConfig, version string) error {
	req := &cloudpb.UpdateRetentionPluginConfigRequest{
		PluginId: clickhousePluginId,
		Configs: map[string]string{
			exportUrlConfig: config.ExportUrl,
		},
		Enabled:         &types.BoolValue{Value: true},
		Version:         &types.StringValue{Value: version},
		CustomExportUrl: &types.StringValue{Value: config.ExportUrl},
		InsecureTLS:     &types.BoolValue{Value: false},
		DisablePresets:  &types.BoolValue{Value: true},
	}
	_, err := c.pluginClient.UpdateRetentionPluginConfig(c.ctx, req)
	return err
}

func (c *Client) GetPresetScripts() ([]*script.ScriptDefinition, error) {
	resp, err := c.pluginClient.GetRetentionScripts(c.ctx, &cloudpb.GetRetentionScriptsRequest{})
	if err != nil {
		return nil, err
	}
	var l []*script.ScriptDefinition
	for _, s := range resp.Scripts {
		if s.PluginId == clickhousePluginId && s.IsPreset {
			sd, err := c.getScriptDefinition(s)
			if err != nil {
				return nil, err
			}
			l = append(l, sd)
		}
	}
	return l, nil
}

func (c *Client) GetClusterScripts(clusterId, clusterName string) ([]*script.Script, error) {
	resp, err := c.pluginClient.GetRetentionScripts(c.ctx, &cloudpb.GetRetentionScriptsRequest{})
	if err != nil {
		return nil, err
	}
	var l []*script.Script
	for _, s := range resp.Scripts {
		if s.PluginId == clickhousePluginId {
			sd, err := c.getScriptDefinition(s)
			if err != nil {
				return nil, err
			}
			l = append(l, &script.Script{
				ScriptDefinition: *sd,
				ScriptId:         utils.ProtoToUUIDStr(s.ScriptID),
				ClusterIds:       getClusterIdsAsString(s.ClusterIDs),
			})
		}
	}
	return l, nil
}

func getClusterIdsAsString(clusterIDs []*uuidpb.UUID) string {
	scriptClusterId := ""
	for i, id := range clusterIDs {
		if i > 0 {
			scriptClusterId = scriptClusterId + ","
		}
		scriptClusterId = scriptClusterId + utils.ProtoToUUIDStr(id)
	}
	return scriptClusterId
}

func (c *Client) getScriptDefinition(s *cloudpb.RetentionScript) (*script.ScriptDefinition, error) {
	resp, err := c.pluginClient.GetRetentionScript(c.ctx, &cloudpb.GetRetentionScriptRequest{ID: s.ScriptID})
	if err != nil {
		return nil, err
	}
	return &script.ScriptDefinition{
		Name:        s.ScriptName,
		Description: s.Description,
		FrequencyS:  s.FrequencyS,
		Script:      resp.Contents,
		IsPreset:    s.IsPreset,
	}, nil
}

func (c *Client) AddDataRetentionScript(clusterId string, scriptName string, description string, frequencyS int64, contents string) error {
	req := &cloudpb.CreateRetentionScriptRequest{
		ScriptName:  scriptName,
		Description: description,
		FrequencyS:  frequencyS,
		Contents:    contents,
		ClusterIDs:  []*uuidpb.UUID{utils.ProtoFromUUIDStrOrNil(clusterId)},
		PluginId:    clickhousePluginId,
	}
	_, err := c.pluginClient.CreateRetentionScript(c.ctx, req)
	return err
}

func (c *Client) UpdateDataRetentionScript(clusterId string, scriptId string, scriptName string, description string, frequencyS int64, contents string) error {
	req := &cloudpb.UpdateRetentionScriptRequest{
		ID:          utils.ProtoFromUUIDStrOrNil(scriptId),
		ScriptName:  &types.StringValue{Value: scriptName},
		Description: &types.StringValue{Value: description},
		Enabled:     &types.BoolValue{Value: true},
		FrequencyS:  &types.Int64Value{Value: frequencyS},
		Contents:    &types.StringValue{Value: contents},
		ClusterIDs:  []*uuidpb.UUID{utils.ProtoFromUUIDStrOrNil(clusterId)},
	}
	_, err := c.pluginClient.UpdateRetentionScript(c.ctx, req)
	return err
}

func (c *Client) DeleteDataRetentionScript(scriptId string) error {
	req := &cloudpb.DeleteRetentionScriptRequest{
		ID: utils.ProtoFromUUIDStrOrNil(scriptId),
	}
	_, err := c.pluginClient.DeleteRetentionScript(c.ctx, req)
	return err
}
