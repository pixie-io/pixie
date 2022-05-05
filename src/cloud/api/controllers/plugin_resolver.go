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
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/utils"
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

// RetentionPluginConfigResolver is the resolver responsible for resolving the full config for a plugin.
type RetentionPluginConfigResolver struct {
	Configs         []*PluginConfigResolver
	CustomExportURL *string
	InsecureTLS     *bool
}

// PluginInfoResolver is the resolver responsible for resolving plugin info.
type PluginInfoResolver struct {
	Configs              []PluginConfigResolver
	AllowCustomExportURL bool
	AllowInsecureTLS     bool
	DefaultExportURL     string
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
		Configs:              configs,
		AllowCustomExportURL: resp.AllowCustomExportURL,
		AllowInsecureTLS:     resp.AllowInsecureTLS,
		DefaultExportURL:     resp.DefaultExportURL,
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

// RetentionPluginConfig lists the configured values for the given retention plugin.
func (q *QueryResolver) RetentionPluginConfig(ctx context.Context, args retentionPluginConfigArgs) (*RetentionPluginConfigResolver, error) {
	configs := make([]*PluginConfigResolver, 0)
	resp, err := q.Env.PluginServer.GetOrgRetentionPluginConfig(ctx, &cloudpb.GetOrgRetentionPluginConfigRequest{
		PluginId: args.ID,
	})

	if err != nil {
		return nil, err
	}

	for k, v := range resp.Configs {
		configs = append(configs, &PluginConfigResolver{
			Name:  k,
			Value: v,
		})
	}

	r := &RetentionPluginConfigResolver{
		Configs:     configs,
		InsecureTLS: &resp.InsecureTLS,
	}

	if resp.CustomExportUrl != "" {
		r.CustomExportURL = &resp.CustomExportUrl
	}

	return r, nil
}

type editablePluginConfig struct {
	Name  string
	Value string
}

type editablePluginConfigs struct {
	Configs         []editablePluginConfig
	CustomExportURL *string
	InsecureTLS     *bool
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

	if args.Configs.CustomExportURL != nil {
		req.CustomExportUrl = &types.StringValue{
			Value: *args.Configs.CustomExportURL,
		}
	}

	if args.Configs.InsecureTLS != nil {
		req.InsecureTLS = &types.BoolValue{
			Value: *args.Configs.InsecureTLS,
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
	IsPreset        bool
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

type deleteRetentionScriptArgs struct {
	ID graphql.ID
}

type retentionScriptArgs struct {
	ID string
}

type createRetentionScriptArgs struct {
	Script *editableRetentionScript
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
	resp, err := q.Env.PluginServer.GetRetentionScripts(ctx, &cloudpb.GetRetentionScriptsRequest{})
	if err != nil {
		return make([]*RetentionScriptResolver, 0), err
	}

	scripts := make([]*RetentionScriptResolver, len(resp.Scripts))

	for i, s := range resp.Scripts {
		clusterIDs := make([]uuid.UUID, len(s.ClusterIDs))
		for j, c := range s.ClusterIDs {
			clusterIDs[j] = utils.UUIDFromProtoOrNil(c)
		}

		scripts[i] = &RetentionScriptResolver{
			scriptID:    utils.UUIDFromProtoOrNil(s.ScriptID),
			Name:        s.ScriptName,
			Description: s.Description,
			FrequencyS:  int32(s.FrequencyS),
			clusterIDs:  clusterIDs,
			PluginID:    s.PluginId,
			Enabled:     s.Enabled,
			IsPreset:    s.IsPreset,
		}
	}

	return scripts, nil
}

// RetentionScript fetches a single retention script, given an ID.
func (q *QueryResolver) RetentionScript(ctx context.Context, args retentionScriptArgs) (*RetentionScriptResolver, error) {
	resp, err := q.Env.PluginServer.GetRetentionScript(ctx, &cloudpb.GetRetentionScriptRequest{
		ID: utils.ProtoFromUUIDStrOrNil(args.ID),
	})
	if err != nil {
		return nil, err
	}

	s := resp.Script
	clusterIDs := make([]uuid.UUID, len(s.ClusterIDs))
	for j, c := range s.ClusterIDs {
		clusterIDs[j] = utils.UUIDFromProtoOrNil(c)
	}
	script := &RetentionScriptResolver{
		scriptID:        uuid.FromStringOrNil(args.ID),
		Name:            s.ScriptName,
		Description:     s.Description,
		FrequencyS:      int32(s.FrequencyS),
		Enabled:         s.Enabled,
		Contents:        resp.Contents,
		PluginID:        s.PluginId,
		CustomExportURL: &resp.ExportURL,
		clusterIDs:      clusterIDs,
		IsPreset:        s.IsPreset,
	}

	return script, nil
}

// UpdateRetentionScript updates the details for a single retention script.
func (q *QueryResolver) UpdateRetentionScript(ctx context.Context, args updateRetentionScriptArgs) (bool, error) {
	req := &cloudpb.UpdateRetentionScriptRequest{
		ID: utils.ProtoFromUUIDStrOrNil(string(args.ID)),
	}

	if args.Script == nil {
		return false, errors.New("Nothing to update")
	}
	script := args.Script

	clusterIDs := make([]*uuidpb.UUID, 0)

	if script.Clusters != nil {
		for _, c := range *script.Clusters {
			clusterIDs = append(clusterIDs, utils.ProtoFromUUIDStrOrNil(c))
		}
	}
	req.ClusterIDs = clusterIDs

	if script.PluginID != nil {
		return false, errors.New("Updating plugins for a script is currently unsupported")
	}

	if script.Name != nil {
		req.ScriptName = &types.StringValue{Value: *script.Name}
	}

	if script.Description != nil {
		req.Description = &types.StringValue{Value: *script.Description}
	}

	if script.FrequencyS != nil {
		req.FrequencyS = &types.Int64Value{Value: int64(*script.FrequencyS)}
	}

	if script.Enabled != nil {
		req.Enabled = &types.BoolValue{Value: *script.Enabled}
	}

	if script.Contents != nil {
		req.Contents = &types.StringValue{Value: *script.Contents}
	}

	if script.CustomExportURL != nil {
		req.ExportUrl = &types.StringValue{Value: *script.CustomExportURL}
	}

	_, err := q.Env.PluginServer.UpdateRetentionScript(ctx, req)
	if err != nil {
		return false, err
	}

	return true, err
}

// DeleteRetentionScript deletes a retention script.
func (q *QueryResolver) DeleteRetentionScript(ctx context.Context, args deleteRetentionScriptArgs) (bool, error) {
	req := &cloudpb.DeleteRetentionScriptRequest{
		ID: utils.ProtoFromUUIDStrOrNil(string(args.ID)),
	}

	_, err := q.Env.PluginServer.DeleteRetentionScript(ctx, req)
	if err != nil {
		return false, err
	}

	return true, nil
}

// CreateRetentionScript creates a new retention script.
func (q *QueryResolver) CreateRetentionScript(ctx context.Context, args createRetentionScriptArgs) (graphql.ID, error) {
	req := &cloudpb.CreateRetentionScriptRequest{}

	if args.Script == nil {
		return graphql.ID(""), errors.New("Nothing to create")
	}
	script := args.Script

	clusterIDs := make([]*uuidpb.UUID, 0)

	if script.Clusters != nil {
		for _, c := range *script.Clusters {
			clusterIDs = append(clusterIDs, utils.ProtoFromUUIDStrOrNil(c))
		}
	}
	req.ClusterIDs = clusterIDs

	if script.PluginID == nil {
		return graphql.ID(""), errors.New("Cannot create a retention script without a plugin")
	}

	req.PluginId = *script.PluginID

	if script.Name != nil {
		req.ScriptName = *script.Name
	}

	if script.Description != nil {
		req.Description = *script.Description
	}

	if script.FrequencyS != nil {
		req.FrequencyS = int64(*script.FrequencyS)
	}

	if script.Contents != nil {
		req.Contents = *script.Contents
	}

	if script.CustomExportURL != nil {
		req.ExportUrl = *script.CustomExportURL
	}

	resp, err := q.Env.PluginServer.CreateRetentionScript(ctx, req)
	if err != nil {
		return graphql.ID(""), err
	}

	return graphql.ID(utils.UUIDFromProtoOrNil(resp.ID).String()), nil
}
