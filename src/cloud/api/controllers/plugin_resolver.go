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

package controllers

import (
	"context"
	"errors"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/graph-gophers/graphql-go"

	"px.dev/pixie/src/api/proto/cloudpb"
)

// PluginResolver is the resolver responsible for resolving plugins.
type PluginResolver struct {
	ID                string
	Name              string
	Description       string
	Logo              *string
	LatestVersion     string
	SupportsRetention bool
	RetentionEnabled  bool
	EnabledVersion    *string
}

type pluginsArgs struct {
	Kind *string
}

type retentionPluginConfigArgs struct {
	ID string
}

type retentionPluginInfoArgs struct {
	ID            string
	PluginVersion string
}

// PluginConfigResolver is the resolver responsible for resolving plugin configs.
type PluginConfigResolver struct {
	Name        string
	Value       string
	Description string
}

// PluginInfoResolver is the resolver responsible for resolving plugin info.
type PluginInfoResolver struct {
	Configs []PluginConfigResolver
}

func kindGQLToCloudProto(kind string) cloudpb.PluginKind {
	switch kind {
	case "PK_RETENTION":
		return cloudpb.PK_RETENTION
	default:
		return cloudpb.PK_UNKNOWN
	}
}

// Plugins lists all of the plugins, filtered by kind if specified.
func (q *QueryResolver) Plugins(ctx context.Context, args pluginsArgs) ([]*PluginResolver, error) {
	kind := ""
	if args.Kind != nil {
		kind = *args.Kind
	}

	plugins := make([]*PluginResolver, 0)

	resp, err := q.Env.PluginServer.GetPlugins(ctx, &cloudpb.GetPluginsRequest{
		Kind: kindGQLToCloudProto(kind),
	})
	if err != nil {
		return plugins, err
	}

	for _, p := range resp.Plugins {
		pr := &PluginResolver{
			ID:                p.Id,
			Name:              p.Name,
			Description:       p.Description,
			LatestVersion:     p.LatestVersion,
			SupportsRetention: p.RetentionSupported,
			RetentionEnabled:  p.RetentionEnabled,
		}
		if p.Logo != "" {
			pr.Logo = &p.Logo
		}
		if p.RetentionEnabled {
			pr.EnabledVersion = &p.EnabledVersion
		}

		plugins = append(plugins, pr)
	}

	return plugins, nil
}

// RetentionPluginInfo lists information about a specific plugin release.
func (q *QueryResolver) RetentionPluginInfo(ctx context.Context, args retentionPluginInfoArgs) (*PluginInfoResolver, error) {
	resp, err := q.Env.PluginServer.GetRetentionPluginInfo(ctx, &cloudpb.GetRetentionPluginInfoRequest{
		PluginId: args.ID,
		Version:  args.PluginVersion,
	})

	if err != nil {
		return nil, err
	}

	configs := make([]PluginConfigResolver, 0)

	for k, v := range resp.Configs {
		configs = append(configs, PluginConfigResolver{
			Name:        k,
			Description: v,
		})
	}

	return &PluginInfoResolver{
		Configs: configs,
	}, nil
}

// RetentionPluginConfig lists the configured values for the given retention plugin.
func (q *QueryResolver) OrgRetentionPluginConfig(ctx context.Context, args retentionPluginConfigArgs) ([]*PluginConfigResolver, error) {
	configs := make([]*PluginConfigResolver, 0)
	resp, err := q.Env.PluginServer.GetOrgRetentionPluginConfig(ctx, &cloudpb.GetOrgRetentionPluginConfigRequest{
		PluginId: args.ID,
	})

	if err != nil {
		return configs, err
	}

	for k, v := range resp.Configs {
		configs = append(configs, &PluginConfigResolver{
			Name:  k,
			Value: v,
		})
	}

	return configs, nil
}

type editablePluginConfig struct {
	Name  string
	Value string
}

type editablePluginConfigs struct {
	Configs []editablePluginConfig
}

type updateRetentionPluginConfigArgs struct {
	ID             string
	Enabled        *bool
	Configs        editablePluginConfigs
	EnabledVersion *string
}

// UpdateRetentionPluginConfig updates the configs for a retention plugin, including enabling/disabling the plugin.
func (q *QueryResolver) UpdateRetentionPluginConfig(ctx context.Context, args updateRetentionPluginConfigArgs) (bool, error) {
	configs := make(map[string]string)
	for _, c := range args.Configs.Configs {
		configs[c.Name] = c.Value
	}

	req := &cloudpb.UpdateRetentionPluginConfigRequest{
		PluginId: args.ID,
		Configs:  configs,
	}

	if args.Enabled != nil {
		req.Enabled = &types.BoolValue{
			Value: *args.Enabled,
		}
	}

	if args.EnabledVersion != nil {
		req.Version = &types.StringValue{
			Value: *args.EnabledVersion,
		}
	}
	_, err := q.Env.PluginServer.UpdateRetentionPluginConfig(ctx, req)

	if err != nil {
		return false, err
	}

	return true, nil
}

// RetentionScriptResolver is responsible for resolving retention plugin scripts.
type RetentionScriptResolver struct {
	scriptID        uuid.UUID
	Name            string
	Description     string
	FrequencyS      int32
	Enabled         bool
	clusterIDs      []uuid.UUID
	Contents        string
	PluginID        string
	CustomExportURL *string
}

type editableRetentionScript struct {
	Name            *string
	Description     *string
	FrequencyS      *int32
	Enabled         *bool
	Contents        *string
	PluginID        *string
	CustomExportURL *string
	Clusters        *[]string
}

type updateRetentionScriptArgs struct {
	ID     graphql.ID
	Script *editableRetentionScript
}

type retentionScriptArgs struct {
	ID string
}

// ID returns cluster ID.
func (c *RetentionScriptResolver) ID() graphql.ID {
	return graphql.ID(c.scriptID.String())
}

// Clusters returns the IDs of the clusters with the script enabled.
func (c *RetentionScriptResolver) Clusters() []graphql.ID {
	ids := make([]graphql.ID, len(c.clusterIDs))
	for i, c := range c.clusterIDs {
		ids[i] = graphql.ID(c.String())
	}
	return ids
}

// RetentionScripts fetches all retention scripts belonging to the org.
func (q *QueryResolver) RetentionScripts(ctx context.Context) ([]*RetentionScriptResolver, error) {
	return make([]*RetentionScriptResolver, 0), errors.New("Not yet implemented")
}

// RetentionScript fetches a single retention script, given an ID.
func (q *QueryResolver) RetentionScript(ctx context.Context, args retentionScriptArgs) (*RetentionScriptResolver, error) {
	return nil, errors.New("Not yet implemented")
}

// UpdateRetentionScript updates the details for a single retention script.
func (q *QueryResolver) UpdateRetentionScript(ctx context.Context, args updateRetentionScriptArgs) (bool, error) {
	return false, errors.New("Not yet implemented")
}
