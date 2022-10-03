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

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/plugin/pluginpb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
)

// PluginServiceServer is used to manage and configure plugins.
type PluginServiceServer struct {
	PluginServiceClient              pluginpb.PluginServiceClient
	DataRetentionPluginServiceClient pluginpb.DataRetentionPluginServiceClient
}

func kindCloudProtoToPluginProto(kind cloudpb.PluginKind) pluginpb.PluginKind {
	switch kind {
	case cloudpb.PK_RETENTION:
		return pluginpb.PLUGIN_KIND_RETENTION
	default:
		return pluginpb.PLUGIN_KIND_UNKNOWN
	}
}

// GetPlugins fetches all of the available plugins and whether the org has the plugin enabled.
func (p *PluginServiceServer) GetPlugins(ctx context.Context, req *cloudpb.GetPluginsRequest) (*cloudpb.GetPluginsResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID := utils.ProtoFromUUIDStrOrNil(orgIDstr)

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	pluginsResp, err := p.PluginServiceClient.GetPlugins(ctx, &pluginpb.GetPluginsRequest{
		Kind: kindCloudProtoToPluginProto(req.Kind),
	})
	if err != nil {
		return nil, err
	}

	orgPlugins, err := p.DataRetentionPluginServiceClient.GetRetentionPluginsForOrg(ctx, &pluginpb.GetRetentionPluginsForOrgRequest{
		OrgID: orgID,
	})
	if err != nil {
		return nil, err
	}
	// Create a map of enabled plugins. Map of pluginID to enabled version.
	enabledPlugins := map[string]string{}
	for _, p := range orgPlugins.Plugins {
		enabledPlugins[p.Plugin.ID] = p.EnabledVersion
	}

	plugins := make([]*cloudpb.Plugin, len(pluginsResp.Plugins))
	for i, p := range pluginsResp.Plugins {
		plugins[i] = &cloudpb.Plugin{
			Name:               p.Name,
			Id:                 p.ID,
			Description:        p.Description,
			Logo:               p.Logo,
			LatestVersion:      p.LatestVersion,
			RetentionSupported: p.RetentionEnabled,
		}

		if v, ok := enabledPlugins[p.ID]; ok {
			plugins[i].RetentionEnabled = true
			plugins[i].EnabledVersion = v
		}
	}

	return &cloudpb.GetPluginsResponse{
		Plugins: plugins,
	}, nil
}

// GetRetentionPluginConfig gets the retention plugin config for a plugin.
func (p *PluginServiceServer) GetOrgRetentionPluginConfig(ctx context.Context, req *cloudpb.GetOrgRetentionPluginConfigRequest) (*cloudpb.GetOrgRetentionPluginConfigResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID := utils.ProtoFromUUIDStrOrNil(orgIDstr)

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	pluginsResp, err := p.DataRetentionPluginServiceClient.GetOrgRetentionPluginConfig(ctx, &pluginpb.GetOrgRetentionPluginConfigRequest{
		PluginID: req.PluginId,
		OrgID:    orgID,
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.GetOrgRetentionPluginConfigResponse{
		Configs:         pluginsResp.Configurations,
		CustomExportUrl: pluginsResp.CustomExportUrl,
		InsecureTLS:     pluginsResp.InsecureTLS,
	}, nil
}

// GetRetentionPluginInfo gets the retention plugin info for a particular plugin release.
func (p *PluginServiceServer) GetRetentionPluginInfo(ctx context.Context, req *cloudpb.GetRetentionPluginInfoRequest) (*cloudpb.GetRetentionPluginInfoResponse, error) {
	var err error
	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	configResp, err := p.PluginServiceClient.GetRetentionPluginConfig(ctx, &pluginpb.GetRetentionPluginConfigRequest{
		ID:      req.PluginId,
		Version: req.Version,
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.GetRetentionPluginInfoResponse{
		Configs:              configResp.Configurations,
		AllowCustomExportURL: configResp.AllowCustomExportURL,
		AllowInsecureTLS:     configResp.AllowInsecureTLS,
		DefaultExportURL:     configResp.DefaultExportURL,
	}, nil
}

// UpdateRetentionPluginConfig updates the retention plugin config for a plugin.
func (p *PluginServiceServer) UpdateRetentionPluginConfig(ctx context.Context, req *cloudpb.UpdateRetentionPluginConfigRequest) (*cloudpb.UpdateRetentionPluginConfigResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID := utils.ProtoFromUUIDStrOrNil(orgIDstr)

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	_, err = p.DataRetentionPluginServiceClient.UpdateOrgRetentionPluginConfig(ctx, &pluginpb.UpdateOrgRetentionPluginConfigRequest{
		PluginID:        req.PluginId,
		OrgID:           orgID,
		Configurations:  req.Configs,
		Enabled:         req.Enabled,
		Version:         req.Version,
		CustomExportUrl: req.CustomExportUrl,
		InsecureTLS:     req.InsecureTLS,
		DisablePresets:  req.DisablePresets,
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.UpdateRetentionPluginConfigResponse{}, nil
}

// GetRetentionScripts gets the retention scripts configured for the org.
func (p *PluginServiceServer) GetRetentionScripts(ctx context.Context, req *cloudpb.GetRetentionScriptsRequest) (*cloudpb.GetRetentionScriptsResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID := utils.ProtoFromUUIDStrOrNil(orgIDstr)

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := p.DataRetentionPluginServiceClient.GetRetentionScripts(ctx, &pluginpb.GetRetentionScriptsRequest{
		OrgID: orgID,
	})
	if err != nil {
		return nil, err
	}

	scripts := make([]*cloudpb.RetentionScript, len(resp.Scripts))
	for i, s := range resp.Scripts {
		scripts[i] = &cloudpb.RetentionScript{
			ScriptID:    s.ScriptID,
			ScriptName:  s.ScriptName,
			Description: s.Description,
			FrequencyS:  s.FrequencyS,
			ClusterIDs:  s.ClusterIDs,
			PluginId:    s.PluginId,
			Enabled:     s.Enabled,
			IsPreset:    s.IsPreset,
		}
	}
	return &cloudpb.GetRetentionScriptsResponse{
		Scripts: scripts,
	}, nil
}

// GetRetentionScript gets detailed information about a specific retention script.
func (p *PluginServiceServer) GetRetentionScript(ctx context.Context, req *cloudpb.GetRetentionScriptRequest) (*cloudpb.GetRetentionScriptResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID := utils.ProtoFromUUIDStrOrNil(orgIDstr)

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := p.DataRetentionPluginServiceClient.GetRetentionScript(ctx, &pluginpb.GetRetentionScriptRequest{
		OrgID:    orgID,
		ScriptID: req.ID,
	})
	if err != nil {
		return nil, err
	}

	scriptDetails := resp.Script.Script
	return &cloudpb.GetRetentionScriptResponse{
		Script: &cloudpb.RetentionScript{
			ScriptID:    scriptDetails.ScriptID,
			ScriptName:  scriptDetails.ScriptName,
			Description: scriptDetails.Description,
			FrequencyS:  scriptDetails.FrequencyS,
			ClusterIDs:  scriptDetails.ClusterIDs,
			PluginId:    scriptDetails.PluginId,
			Enabled:     scriptDetails.Enabled,
			IsPreset:    scriptDetails.IsPreset,
		},
		Contents:  resp.Script.Contents,
		ExportURL: resp.Script.ExportURL,
	}, nil
}

// UpdateRetentionScript updates a specific retention script.
func (p *PluginServiceServer) UpdateRetentionScript(ctx context.Context, req *cloudpb.UpdateRetentionScriptRequest) (*cloudpb.UpdateRetentionScriptResponse, error) {
	var err error
	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	_, err = p.DataRetentionPluginServiceClient.UpdateRetentionScript(ctx, &pluginpb.UpdateRetentionScriptRequest{
		ScriptID:    req.ID,
		ScriptName:  req.ScriptName,
		Description: req.Description,
		Enabled:     req.Enabled,
		FrequencyS:  req.FrequencyS,
		Contents:    req.Contents,
		ExportUrl:   req.ExportUrl,
		ClusterIDs:  req.ClusterIDs,
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.UpdateRetentionScriptResponse{}, nil
}

// CreateRetentionScript creates a retention script.
func (p *PluginServiceServer) CreateRetentionScript(ctx context.Context, req *cloudpb.CreateRetentionScriptRequest) (*cloudpb.CreateRetentionScriptResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID := utils.ProtoFromUUIDStrOrNil(orgIDstr)

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := p.DataRetentionPluginServiceClient.CreateRetentionScript(ctx, &pluginpb.CreateRetentionScriptRequest{
		Script: &pluginpb.DetailedRetentionScript{
			Script: &pluginpb.RetentionScript{
				ScriptName:  req.ScriptName,
				Description: req.Description,
				FrequencyS:  req.FrequencyS,
				ClusterIDs:  req.ClusterIDs,
				PluginId:    req.PluginId,
				Enabled:     !req.Disabled,
				IsPreset:    false,
			},
			Contents:  req.Contents,
			ExportURL: req.ExportUrl,
		},
		OrgID: orgID,
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.CreateRetentionScriptResponse{ID: resp.ID}, nil
}

// DeleteRetentionScript deletes a specific retention script.
func (p *PluginServiceServer) DeleteRetentionScript(ctx context.Context, req *cloudpb.DeleteRetentionScriptRequest) (*cloudpb.DeleteRetentionScriptResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID := utils.ProtoFromUUIDStrOrNil(orgIDstr)

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	_, err = p.DataRetentionPluginServiceClient.DeleteRetentionScript(ctx, &pluginpb.DeleteRetentionScriptRequest{
		ID:    req.ID,
		OrgID: orgID,
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.DeleteRetentionScriptResponse{}, nil
}
