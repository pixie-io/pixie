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

	"github.com/gogo/protobuf/types"

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
