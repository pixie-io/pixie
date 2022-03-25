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
	"database/sql"
	"encoding/json"
	"fmt"
	"sync"

	"github.com/gofrs/uuid"
	"github.com/jmoiron/sqlx"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"gopkg.in/yaml.v2"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/cron_script/cronscriptpb"
	"px.dev/pixie/src/cloud/plugin/pluginpb"
	"px.dev/pixie/src/shared/scripts"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
)

// Server is a bridge implementation of the pluginService.
type Server struct {
	db    *sqlx.DB
	dbKey string

	cronScriptClient cronscriptpb.CronScriptServiceClient

	done chan struct{}
	once sync.Once
}

// New creates a new server.
func New(db *sqlx.DB, dbKey string, cronScriptClient cronscriptpb.CronScriptServiceClient) *Server {
	return &Server{
		db:               db,
		dbKey:            dbKey,
		cronScriptClient: cronScriptClient,
		done:             make(chan struct{}),
	}
}

// Stop performs any necessary cleanup before shutdown.
func (s *Server) Stop() {
	s.once.Do(func() {
		close(s.done)
	})
}

// PluginService implementation.

// Plugin contains metadata about a plugin.
type Plugin struct {
	Name                 string  `db:"name"`
	ID                   string  `db:"id"`
	Description          *string `db:"description"`
	Logo                 *string `db:"logo"`
	Version              string  `db:"version"`
	DataRetentionEnabled bool    `db:"data_retention_enabled"`
}

// GetPlugins fetches all of the available, latest plugins.
func (s *Server) GetPlugins(ctx context.Context, req *pluginpb.GetPluginsRequest) (*pluginpb.GetPluginsResponse, error) {
	query := `SELECT t1.name, t1.id, t1.description, t1.logo, t1.version, t1.data_retention_enabled FROM plugin_releases t1
		JOIN (SELECT id, MAX(version) as version FROM plugin_releases GROUP BY id) t2
	  	ON t1.id = t2.id AND t1.version = t2.version`

	if req.Kind == pluginpb.PLUGIN_KIND_RETENTION {
		query = fmt.Sprintf("%s %s", query, "WHERE data_retention_enabled='true'")
	}

	rows, err := s.db.Queryx(query)
	if err != nil {
		if err == sql.ErrNoRows {
			return &pluginpb.GetPluginsResponse{Plugins: nil}, nil
		}
		return nil, status.Errorf(codes.Internal, "Failed to fetch plugins")
	}
	defer rows.Close()

	plugins := []*pluginpb.Plugin{}
	for rows.Next() {
		var p Plugin
		err = rows.StructScan(&p)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read plugins")
		}
		ppb := &pluginpb.Plugin{
			Name:             p.Name,
			ID:               p.ID,
			LatestVersion:    p.Version,
			RetentionEnabled: p.DataRetentionEnabled,
		}
		if p.Description != nil {
			ppb.Description = *p.Description
		}
		if p.Logo != nil {
			ppb.Logo = *p.Logo
		}
		plugins = append(plugins, ppb)
	}
	return &pluginpb.GetPluginsResponse{Plugins: plugins}, nil
}

// RetentionPlugin contains metadata about a retention plugin.
type RetentionPlugin struct {
	ID                   string         `db:"plugin_id"`
	Version              string         `db:"version"`
	Configurations       Configurations `db:"configurations"`
	DocumentationURL     *string        `db:"documentation_url"`
	DefaultExportURL     *string        `db:"default_export_url"`
	AllowCustomExportURL bool           `db:"allow_custom_export_url"`
	PresetScripts        PresetScripts  `db:"preset_scripts"`
}

// GetRetentionPluginConfig gets the config for a specific plugin release.
func (s *Server) GetRetentionPluginConfig(ctx context.Context, req *pluginpb.GetRetentionPluginConfigRequest) (*pluginpb.GetRetentionPluginConfigResponse, error) {
	query := `SELECT plugin_id, version, configurations, preset_scripts, documentation_url, default_export_url, allow_custom_export_url FROM data_retention_plugin_releases WHERE plugin_id=$1 AND version=$2`
	rows, err := s.db.Queryx(query, req.ID, req.Version)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch plugin")
	}
	defer rows.Close()

	if rows.Next() {
		var plugin RetentionPlugin
		err := rows.StructScan(&plugin)
		if err != nil {
			return nil, status.Errorf(codes.Internal, "Failed to read plugin")
		}
		ppb := &pluginpb.GetRetentionPluginConfigResponse{
			Configurations:       plugin.Configurations,
			AllowCustomExportURL: plugin.AllowCustomExportURL,
			PresetScripts:        []*pluginpb.GetRetentionPluginConfigResponse_PresetScript{},
		}
		if plugin.DocumentationURL != nil {
			ppb.DocumentationURL = *plugin.DocumentationURL
		}
		if plugin.DefaultExportURL != nil {
			ppb.DefaultExportURL = *plugin.DefaultExportURL
		}
		if plugin.PresetScripts != nil {
			for _, p := range plugin.PresetScripts {
				ppb.PresetScripts = append(ppb.PresetScripts, &pluginpb.GetRetentionPluginConfigResponse_PresetScript{
					Name:              p.Name,
					Description:       p.Description,
					DefaultFrequencyS: p.DefaultFrequencyS,
					Script:            p.Script,
				})
			}
		}
		return ppb, nil
	}
	return nil, status.Error(codes.NotFound, "plugin not found")
}

// GetRetentionPluginsForOrg gets all data retention plugins enabled by the org.
func (s *Server) GetRetentionPluginsForOrg(ctx context.Context, req *pluginpb.GetRetentionPluginsForOrgRequest) (*pluginpb.GetRetentionPluginsForOrgResponse, error) {
	query := `SELECT r.name, r.id, r.description, r.logo, r.version, r.data_retention_enabled from plugin_releases as r, org_data_retention_plugins as o WHERE r.id = o.plugin_id AND r.version = o.version AND org_id=$1`
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	rows, err := s.db.Queryx(query, orgID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch plugin")
	}

	defer rows.Close()

	plugins := []*pluginpb.GetRetentionPluginsForOrgResponse_PluginState{}
	for rows.Next() {
		var p Plugin
		err = rows.StructScan(&p)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read plugins")
		}
		ppb := &pluginpb.GetRetentionPluginsForOrgResponse_PluginState{
			Plugin: &pluginpb.Plugin{
				Name:             p.Name,
				ID:               p.ID,
				RetentionEnabled: p.DataRetentionEnabled,
			},
			EnabledVersion: p.Version,
		}
		plugins = append(plugins, ppb)
	}
	return &pluginpb.GetRetentionPluginsForOrgResponse{Plugins: plugins}, nil
}

// GetOrgRetentionPluginConfig gets the org's configuration for a plugin.
func (s *Server) GetOrgRetentionPluginConfig(ctx context.Context, req *pluginpb.GetOrgRetentionPluginConfigRequest) (*pluginpb.GetOrgRetentionPluginConfigResponse, error) {
	query := `SELECT PGP_SYM_DECRYPT(configurations, $1::text) FROM org_data_retention_plugins WHERE org_id=$2 AND plugin_id=$3`

	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	rows, err := s.db.Queryx(query, s.dbKey, orgID, req.PluginID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch plugin")
	}
	defer rows.Close()

	if rows.Next() {
		var configurationJSON []byte
		var configMap map[string]string

		err := rows.Scan(&configurationJSON)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read configs")
		}

		err = json.Unmarshal(configurationJSON, &configMap)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read configs")
		}

		return &pluginpb.GetOrgRetentionPluginConfigResponse{
			Configurations: configMap,
		}, nil
	}
	return nil, status.Error(codes.NotFound, "plugin is not enabled")
}

func (s *Server) enableOrgRetention(orgID uuid.UUID, pluginID string, version string, configurations []byte) error {
	query := `INSERT INTO org_data_retention_plugins (org_id, plugin_id, version, configurations) VALUES ($1, $2, $3, PGP_SYM_ENCRYPT($4, $5))`

	// TODO(michelle): Enabling org retention should enable any preset scripts.

	_, err := s.db.Exec(query, orgID, pluginID, version, configurations, s.dbKey)
	return err
}

func (s *Server) disableOrgRetention(orgID uuid.UUID, pluginID string) error {
	query := `DELETE FROM org_data_retention_plugins WHERE org_id=$1 AND plugin_id=$2`

	// TODO(michelle): Disabling org retention should disable/delete any retention scripts.

	_, err := s.db.Exec(query, orgID, pluginID)
	return err
}

func (s *Server) updateOrgRetentionConfigs(orgID uuid.UUID, pluginID string, version string, configurations []byte) error {
	query := `UPDATE org_data_retention_plugins SET version = $1, configurations = PGP_SYM_ENCRYPT($2, $3) WHERE org_id = $4 AND plugin_id = $5`

	// TODO(michelle): Updating configs should update any retention script YAMLs.

	_, err := s.db.Exec(query, version, configurations, s.dbKey, orgID, pluginID)
	return err
}

// UpdateOrgRetentionPluginConfig updates an org's configuration for a plugin.
func (s *Server) UpdateOrgRetentionPluginConfig(ctx context.Context, req *pluginpb.UpdateOrgRetentionPluginConfigRequest) (*pluginpb.UpdateOrgRetentionPluginConfigResponse, error) {
	if utils.IsNilUUIDProto(req.OrgID) {
		return nil, status.Error(codes.InvalidArgument, "Must specify OrgID")
	}

	if req.PluginID == "" {
		return nil, status.Error(codes.InvalidArgument, "Must specify plugin ID")
	}

	if req.Enabled != nil && req.Enabled.Value && req.Version == nil {
		return nil, status.Error(codes.InvalidArgument, "Must specify plugin version when enabling")
	}

	var configurations []byte
	var version string

	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	if req.Version != nil {
		version = req.Version.Value
	}
	if req.Configurations != nil && len(req.Configurations) > 0 {
		configurations, _ = json.Marshal(req.Configurations)
	}

	if req.Enabled != nil && req.Enabled.Value { // Plugin was just enabled, we should create it.
		return &pluginpb.UpdateOrgRetentionPluginConfigResponse{}, s.enableOrgRetention(orgID, req.PluginID, version, configurations)
	} else if req.Enabled != nil && !req.Enabled.Value { // Plugin was disabled, we should delete it.
		return &pluginpb.UpdateOrgRetentionPluginConfigResponse{}, s.disableOrgRetention(orgID, req.PluginID)
	}

	// Fetch current configs.
	query := `SELECT version, PGP_SYM_DECRYPT(configurations, $1::text) FROM org_data_retention_plugins WHERE org_id=$2 AND plugin_id=$3`
	rows, err := s.db.Queryx(query, s.dbKey, orgID, req.PluginID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch plugin")
	}
	defer rows.Close()

	var origConfig []byte
	var origVersion string
	if rows.Next() {
		err := rows.Scan(&origVersion, &origConfig)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read configs")
		}
	}

	if configurations == nil {
		configurations = origConfig
	}
	if version == "" {
		version = origVersion
	}

	err = s.updateOrgRetentionConfigs(orgID, req.PluginID, version, configurations)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to update configs")
	}

	// if origVersion != version { // The user is updating the plugin.
	// 	// TODO(michelle): If the user is updating the plugin, we may need to update some of the presetScripts users have configured.
	// }

	return &pluginpb.UpdateOrgRetentionPluginConfigResponse{}, nil
}

// RetentionScript represents a retention script in the plugin system.
type RetentionScript struct {
	OrgID       uuid.UUID `db:"org_id"`
	ScriptID    uuid.UUID `db:"script_id"`
	ScriptName  string    `db:"script_name"`
	Description string    `db:"description"`
	IsPreset    bool      `db:"is_preset"`
	PluginID    string    `db:"plugin_id"`
	ExportURL   string    `db:"export_url"`
}

// GetRetentionScripts gets all retention scripts the org has configured.
func (s *Server) GetRetentionScripts(ctx context.Context, req *pluginpb.GetRetentionScriptsRequest) (*pluginpb.GetRetentionScriptsResponse, error) {
	query := `SELECT script_id, script_name, description, is_preset, plugin_id from plugin_retention_scripts WHERE org_id=$1`
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	rows, err := s.db.Queryx(query, orgID)
	if err != nil {
		log.WithError(err).Error("Failed to fetch scripts")
		return nil, status.Errorf(codes.Internal, "Failed to fetch scripts")
	}

	defer rows.Close()

	scriptMap := map[uuid.UUID]*pluginpb.RetentionScript{}
	scriptIDs := make([]*uuidpb.UUID, 0)
	for rows.Next() {
		var rs RetentionScript
		err = rows.StructScan(&rs)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read script")
		}
		id := utils.ProtoFromUUID(rs.ScriptID)
		scriptMap[rs.ScriptID] = &pluginpb.RetentionScript{
			ScriptID:    id,
			ScriptName:  rs.ScriptName,
			Description: rs.Description,
			PluginId:    rs.PluginID,
			IsPreset:    rs.IsPreset,
		}
		scriptIDs = append(scriptIDs, id)
	}
	cronScriptsResp, err := s.cronScriptClient.GetScripts(ctx, &cronscriptpb.GetScriptsRequest{IDs: scriptIDs})
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch cron scripts")
	}

	for _, c := range cronScriptsResp.Scripts {
		if v, ok := scriptMap[utils.UUIDFromProtoOrNil(c.ID)]; ok {
			v.FrequencyS = c.FrequencyS
			v.Enabled = c.Enabled
			v.ClusterIDs = c.ClusterIDs
		}
	}

	scripts := make([]*pluginpb.RetentionScript, 0)
	for _, v := range scriptMap {
		scripts = append(scripts, v)
	}
	return &pluginpb.GetRetentionScriptsResponse{Scripts: scripts}, nil
}

// GetRetentionScript gets the details for a script an org is using for long-term data retention.
func (s *Server) GetRetentionScript(ctx context.Context, req *pluginpb.GetRetentionScriptRequest) (*pluginpb.GetRetentionScriptResponse, error) {
	query := `SELECT script_name, description, is_preset, plugin_id, PGP_SYM_DECRYPT(export_url, $1::text) as export_url from plugin_retention_scripts WHERE org_id=$2 AND script_id=$3`
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	scriptID := utils.UUIDFromProtoOrNil(req.ScriptID)
	rows, err := s.db.Queryx(query, s.dbKey, orgID, scriptID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch script")
	}

	defer rows.Close()

	if !rows.Next() {
		return nil, status.Error(codes.NotFound, "script not found")
	}

	var script RetentionScript
	err = rows.StructScan(&script)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to read script")
	}

	cronScriptResp, err := s.cronScriptClient.GetScript(ctx, &cronscriptpb.GetScriptRequest{ID: req.ScriptID})
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch cron script")
	}
	cronScript := cronScriptResp.Script

	return &pluginpb.GetRetentionScriptResponse{
		Script: &pluginpb.DetailedRetentionScript{
			Script: &pluginpb.RetentionScript{
				ScriptID:    req.ScriptID,
				ScriptName:  script.ScriptName,
				Description: script.Description,
				FrequencyS:  cronScript.FrequencyS,
				ClusterIDs:  cronScript.ClusterIDs,
				PluginId:    script.PluginID,
				Enabled:     cronScript.Enabled,
				IsPreset:    script.IsPreset,
			},
			Contents:  cronScript.Script,
			ExportURL: script.ExportURL,
		},
	}, nil
}

// CreateRetentionScript creates a script that is used for long-term data retention.
func (s *Server) CreateRetentionScript(ctx context.Context, req *pluginpb.CreateRetentionScriptRequest) (*pluginpb.CreateRetentionScriptResponse, error) {
	// Fetch config + headers from plugin info.
	query := `SELECT PGP_SYM_DECRYPT(o.configurations, $1::text), r.default_export_url, r.version FROM org_data_retention_plugins o, data_retention_plugin_releases r WHERE org_id=$2 AND r.plugin_id=$3 AND o.plugin_id=r.plugin_id AND r.version = o.version`
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	rows, err := s.db.Queryx(query, s.dbKey, orgID, req.Script.Script.PluginId)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to fetch plugin")
	}
	defer rows.Close()

	if !rows.Next() {
		return nil, status.Error(codes.NotFound, "plugin is not enabled")
	}

	var configurationJSON []byte
	var configMap map[string]string
	var defaultExportURL string
	var pluginVersion string
	err = rows.Scan(&configurationJSON, &defaultExportURL, &pluginVersion)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to read configs")
	}

	err = json.Unmarshal(configurationJSON, &configMap)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to read configs")
	}

	exportURL := defaultExportURL
	if req.Script.ExportURL != "" {
		exportURL = req.Script.ExportURL
	}

	// Convert endpoint info to string YAML.
	config := &scripts.Config{
		OtelEndpointConfig: &scripts.OtelEndpointConfig{
			URL:     exportURL,
			Headers: configMap,
		},
	}

	mConfig, err := yaml.Marshal(&config)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to marshal configs")
	}

	cronScriptResp, err := s.cronScriptClient.CreateScript(ctx, &cronscriptpb.CreateScriptRequest{
		Script:     req.Script.Contents,
		ClusterIDs: req.Script.Script.ClusterIDs,
		Configs:    string(mConfig),
		FrequencyS: req.Script.Script.FrequencyS,
	})
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to create cron script")
	}

	scriptID := cronScriptResp.ID

	query = `INSERT INTO plugin_retention_scripts (org_id, plugin_id, plugin_version, script_id, script_name, description, export_url, is_preset) VALUES ($1, $2, $3, $4, $5, $6, PGP_SYM_ENCRYPT($7, $8), $9)`
	_, err = s.db.Exec(query, orgID, req.Script.Script.PluginId, pluginVersion, utils.UUIDFromProtoOrNil(scriptID), req.Script.Script.ScriptName, req.Script.Script.Description, exportURL, s.dbKey, req.Script.Script.IsPreset)
	if err != nil {
		log.WithError(err).Error("Failed")

		return nil, status.Errorf(codes.Internal, "Failed to create retention script")
	}

	return &pluginpb.CreateRetentionScriptResponse{}, nil
}

// UpdateRetentionScript updates a script used for long-term data retention.
func (s *Server) UpdateRetentionScript(ctx context.Context, req *pluginpb.UpdateRetentionScriptRequest) (*pluginpb.UpdateRetentionScriptResponse, error) {
	// Fetch existing script.
	query := `SELECT org_id, script_name, description, PGP_SYM_DECRYPT(export_url, $1::text) as export_url from plugin_retention_scripts WHERE script_id=$2`
	scriptID := utils.UUIDFromProtoOrNil(req.ScriptID)
	rows, err := s.db.Queryx(query, s.dbKey, scriptID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch script")
	}
	defer rows.Close()

	if !rows.Next() {
		return nil, status.Error(codes.NotFound, "script not found")
	}

	var script RetentionScript
	err = rows.StructScan(&script)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to read script")
	}

	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthenticated")
	}
	claimsOrgIDstr := sCtx.Claims.GetUserClaims().OrgID
	if script.OrgID.String() != claimsOrgIDstr {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthorized")
	}

	scriptName := script.ScriptName
	description := script.Description
	exportURL := script.ExportURL

	if req.ScriptName != nil {
		scriptName = req.ScriptName.Value
	}
	if req.Description != nil {
		description = req.Description.Value
	}
	if req.ExportUrl != nil {
		exportURL = req.ExportUrl.Value
	}

	query = `UPDATE plugin_retention_scripts SET script_name = $1, export_url = PGP_SYM_ENCRYPT($2, $3), description = $4 WHERE script_id = $5`

	_, err = s.db.Exec(query, scriptName, exportURL, s.dbKey, description, scriptID)

	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to update retention script")
	}

	// Update cron script.
	_, err = s.cronScriptClient.UpdateScript(ctx, &cronscriptpb.UpdateScriptRequest{
		Script:     req.Contents,
		ClusterIDs: req.ClusterIDs,
		Enabled:    req.Enabled,
		FrequencyS: req.FrequencyS,
		ScriptId:   req.ScriptID,
	})
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to update cron script")
	}

	return &pluginpb.UpdateRetentionScriptResponse{}, nil
}
