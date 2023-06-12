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
	"github.com/gogo/protobuf/types"
	"github.com/jmoiron/sqlx"
	"github.com/segmentio/analytics-go/v3"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"gopkg.in/yaml.v2"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/cron_script/cronscriptpb"
	"px.dev/pixie/src/cloud/plugin/pluginpb"
	"px.dev/pixie/src/shared/scripts"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/shared/services/events"
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

func contextWithAuthToken(ctx context.Context) (context.Context, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	return metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", sCtx.AuthToken)), nil
}

// PluginService implementation.

// Plugin contains metadata about a plugin.
type Plugin struct {
	Name                 string  `db:"name"`
	ID                   string  `db:"id"`
	Description          *string `db:"description"`
	Logo                 *string `db:"logo"`
	Version              string  `db:"version"`
	DataRetentionEnabled bool    `db:"data_retention_enabled" yaml:"dataRetentionEnabled"`
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
	DocumentationURL     *string        `db:"documentation_url" yaml:"documentationURL"`
	DefaultExportURL     *string        `db:"default_export_url" yaml:"defaultExportURL"`
	AllowCustomExportURL bool           `db:"allow_custom_export_url" yaml:"allowCustomExportURL"`
	AllowInsecureTLS     bool           `db:"allow_insecure_tls" yaml:"allowInsecureTLS"`
	PresetScripts        PresetScripts  `db:"preset_scripts" yaml:"presetScripts"`
}

// GetRetentionPluginConfig gets the config for a specific plugin release.
func (s *Server) GetRetentionPluginConfig(ctx context.Context, req *pluginpb.GetRetentionPluginConfigRequest) (*pluginpb.GetRetentionPluginConfigResponse, error) {
	query := `SELECT plugin_id, version, configurations, preset_scripts, documentation_url, default_export_url, allow_custom_export_url, allow_insecure_tls FROM data_retention_plugin_releases WHERE plugin_id=$1 AND version=$2`
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
			AllowInsecureTLS:     plugin.AllowInsecureTLS,
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
	query := `SELECT PGP_SYM_DECRYPT(configurations, $1::text), PGP_SYM_DECRYPT(custom_export_url, $1::text), insecure_tls FROM org_data_retention_plugins WHERE org_id=$2 AND plugin_id=$3`

	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	rows, err := s.db.Queryx(query, s.dbKey, orgID, req.PluginID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch plugin")
	}
	defer rows.Close()

	if rows.Next() {
		var configurationJSON []byte
		var exportURL *string
		var insecureTLS bool
		var configMap map[string]string

		err := rows.Scan(&configurationJSON, &exportURL, &insecureTLS)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read configs")
		}

		if len(configurationJSON) > 0 {
			err = json.Unmarshal(configurationJSON, &configMap)
			if err != nil {
				return nil, status.Error(codes.Internal, "failed to read configs")
			}
		}

		resp := &pluginpb.GetOrgRetentionPluginConfigResponse{
			Configurations: configMap,
			InsecureTLS:    insecureTLS,
		}

		if exportURL != nil {
			resp.CustomExportUrl = *exportURL
		}

		return resp, nil
	}
	return nil, status.Error(codes.NotFound, "plugin is not enabled")
}

func (s *Server) enableOrgRetention(ctx context.Context, txn *sqlx.Tx, orgID uuid.UUID, pluginID string, version string, configurations []byte, customExportURL *string, insecureTLS bool, disablePresets bool) error {
	query := `INSERT INTO org_data_retention_plugins (org_id, plugin_id, version, configurations, custom_export_url, insecure_tls) VALUES ($1, $2, $3, PGP_SYM_ENCRYPT($4, $5), PGP_SYM_ENCRYPT($6, $5), $7)`
	_, err := txn.Exec(query, orgID, pluginID, version, configurations, s.dbKey, customExportURL, insecureTLS)
	if err != nil {
		return status.Errorf(codes.Internal, "Failed to create plugin for org")
	}

	return s.createPresetScripts(ctx, txn, orgID, pluginID, version, configurations, customExportURL, disablePresets)
}

func (s *Server) createPresetScripts(ctx context.Context, txn *sqlx.Tx, orgID uuid.UUID, pluginID string, version string, configurations []byte, customExportURL *string, disablePresets bool) error {
	// Enabling org retention should enable any preset scripts.
	query := `SELECT preset_scripts FROM data_retention_plugin_releases WHERE plugin_id=$1 AND version=$2`
	rows, err := txn.Queryx(query, pluginID, version)
	if err != nil {
		return status.Errorf(codes.Internal, "Failed to fetch plugin")
	}

	defer rows.Close()
	var plugin RetentionPlugin
	if rows.Next() {
		err := rows.StructScan(&plugin)
		if err != nil {
			return status.Errorf(codes.Internal, "Failed to read plugin release")
		}
	}
	rows.Close()

	for _, j := range plugin.PresetScripts {
		_, err = s.createRetentionScript(ctx, txn, orgID, pluginID, &RetentionScript{
			ScriptName:  j.Name,
			Description: j.Description,
			IsPreset:    true,
			ExportURL:   "",
		}, j.Script, make([]*uuidpb.UUID, 0), j.DefaultFrequencyS, j.DefaultDisabled || disablePresets)
		if err != nil {
			return status.Errorf(codes.Internal, "Failed to create preset scripts")
		}
	}
	return err
}

func (s *Server) disableOrgRetention(ctx context.Context, txn *sqlx.Tx, orgID uuid.UUID, pluginID string) error {
	// Disabling org retention should delete any preset scripts.
	query := `DELETE from plugin_retention_scripts WHERE org_id=$1 AND plugin_id=$2 AND is_preset=true RETURNING script_id`
	rows, err := txn.Queryx(query, orgID, pluginID)
	if err != nil {
		return status.Errorf(codes.Internal, "Failed to fetch scripts")
	}
	defer rows.Close()
	for rows.Next() {
		var id uuid.UUID
		err = rows.Scan(&id)
		if err != nil {
			continue
		}
		_, err = s.cronScriptClient.DeleteScript(ctx, &cronscriptpb.DeleteScriptRequest{
			ID:    utils.ProtoFromUUID(id),
			OrgID: utils.ProtoFromUUID(orgID),
		})
		if err != nil {
			return status.Errorf(codes.Internal, "Failed to disable script")
		}
	}
	rows.Close()

	// Disable any custom scripts.
	query = `SELECT script_id from plugin_retention_scripts WHERE org_id=$1 AND plugin_id=$2`
	rows, err = txn.Queryx(query, orgID, pluginID)
	if err != nil {
		return status.Errorf(codes.Internal, "Failed to fetch custom export scripts")
	}
	defer rows.Close()
	for rows.Next() {
		var id uuid.UUID
		err = rows.Scan(&id)
		if err != nil {
			continue
		}
		_, err = s.cronScriptClient.UpdateScript(ctx, &cronscriptpb.UpdateScriptRequest{
			ScriptId: utils.ProtoFromUUID(id),
			OrgID:    utils.ProtoFromUUID(orgID),
			Enabled: &types.BoolValue{
				Value: false,
			},
		})
		if err != nil {
			return status.Errorf(codes.Internal, "Failed to disable custom script")
		}
	}
	rows.Close()

	query = `DELETE FROM org_data_retention_plugins WHERE org_id=$1 AND plugin_id=$2`
	_, err = txn.Exec(query, orgID, pluginID)
	return err
}

// updatePresetScripts updates the contents of existing preset scripts, creates any new preset scripts in the new plugin version, and removes any preset scripts
// from the old plugin version.
func (s *Server) updatePresetScripts(ctx context.Context, txn *sqlx.Tx, orgID uuid.UUID, pluginID string, version string, configurations []byte, customExportURL *string, disablePresets bool) error {
	// Fetch preset scripts in new version.
	query := `SELECT preset_scripts FROM data_retention_plugin_releases WHERE plugin_id=$1 AND version=$2`
	rows, err := txn.Queryx(query, pluginID, version)
	if err != nil {
		return status.Errorf(codes.Internal, "Failed to fetch plugin")
	}
	defer rows.Close()
	var plugin RetentionPlugin
	if rows.Next() {
		err := rows.StructScan(&plugin)
		if err != nil {
			return status.Errorf(codes.Internal, "Failed to read plugin release")
		}
	}
	rows.Close()

	// Fetch existing preset scripts from old version.
	query = `SELECT script_id, script_name from plugin_retention_scripts WHERE org_id=$1 AND plugin_id=$2 AND is_preset=true`
	rows2, err := txn.Queryx(query, orgID.String(), pluginID)
	if err != nil {
		return status.Errorf(codes.Internal, "Failed to fetch existing preset scripts")
	}

	existingScripts := make(map[string]uuid.UUID)
	var scriptID uuid.UUID
	var scriptName string
	for rows2.Next() {
		err = rows2.Scan(&scriptID, &scriptName)
		if err != nil {
			continue
		}
		existingScripts[scriptName] = scriptID
	}
	rows2.Close()

	for _, j := range plugin.PresetScripts {
		// If this script already exists, update it. Otherwise, create a new script.
		if i, ok := existingScripts[j.Name]; ok {
			_, err = s.cronScriptClient.UpdateScript(ctx, &cronscriptpb.UpdateScriptRequest{
				ScriptId: utils.ProtoFromUUID(i),
				Script:   &types.StringValue{Value: j.Script},
				OrgID:    utils.ProtoFromUUID(orgID),
			})
			if err != nil {
				return status.Errorf(codes.Internal, "Failed to update preset script")
			}
			// Update description.
			query := `UPDATE plugin_retention_scripts set description=$1 WHERE script_id=$2`
			_, err = txn.Exec(query, j.Description, i)
			if err != nil {
				return err
			}
			delete(existingScripts, j.Name)
		} else {
			_, err = s.createRetentionScript(ctx, txn, orgID, pluginID, &RetentionScript{
				ScriptName:  j.Name,
				Description: j.Description,
				IsPreset:    true,
				ExportURL:   "",
			}, j.Script, make([]*uuidpb.UUID, 0), j.DefaultFrequencyS, disablePresets || j.DefaultDisabled)
			if err != nil {
				return err
			}
		}
	}

	// Remove any old preset scripts.
	oldIDs := make([]string, 0)
	for _, v := range existingScripts {
		oldIDs = append(oldIDs, v.String())
	}

	if len(oldIDs) == 0 {
		return nil
	}

	strQuery := `UPDATE plugin_retention_scripts set is_preset=false WHERE script_id IN (?)`

	query, args, err := sqlx.In(strQuery, oldIDs)
	if err != nil {
		return err
	}
	query = s.db.Rebind(query)
	rows, err = s.db.Queryx(query, args...)
	if err != nil {
		return err
	}
	rows.Close()

	return nil
}

func (s *Server) updateOrgRetentionConfigs(ctx context.Context, txn *sqlx.Tx, orgID uuid.UUID, pluginID string, version string, configurations []byte, customExportURL *string, insecureTLS bool) error {
	query := `UPDATE org_data_retention_plugins SET version = $1, configurations = PGP_SYM_ENCRYPT($2, $3), custom_export_url = PGP_SYM_ENCRYPT($6, $3), insecure_tls=$7 WHERE org_id = $4 AND plugin_id = $5`

	err := s.propagateConfigChangesToScripts(ctx, txn, orgID, pluginID, version, configurations, customExportURL, insecureTLS)
	if err != nil {
		return err
	}

	_, err = txn.Exec(query, version, configurations, s.dbKey, orgID, pluginID, customExportURL, insecureTLS)
	return err
}

func (s *Server) propagateConfigChangesToScripts(ctx context.Context, txn *sqlx.Tx, orgID uuid.UUID, pluginID string, version string, configurations []byte, customExportURL *string, insecureTLS bool) error {
	// Fetch default export URL for plugin.
	pluginExportURL, _, _, err := s.getPluginConfigs(txn, orgID, pluginID)
	if err != nil {
		return err
	}

	if customExportURL != nil {
		pluginExportURL = *customExportURL
	}

	// Fetch all scripts belonging to this plugin.
	query := `SELECT script_id, PGP_SYM_DECRYPT(export_url, $1::text) as export_url from plugin_retention_scripts WHERE org_id=$2 AND plugin_id=$3`
	rows, err := txn.Queryx(query, s.dbKey, orgID, pluginID)
	if err != nil {
		return status.Errorf(codes.Internal, "Failed to fetch scripts")
	}

	rScripts := make([]*RetentionScript, 0)
	for rows.Next() {
		var rs RetentionScript
		err = rows.StructScan(&rs)
		if err != nil {
			continue
		}
		rScripts = append(rScripts, &rs)
	}
	rows.Close()

	// For each script, update with the new config.
	// TODO(michelle): This is a bit inefficient because we issue a call per script. We should consider adding an RPC method for updating multiple scripts.
	for _, sc := range rScripts {
		var configMap map[string]string
		if len(configurations) != 0 {
			err = json.Unmarshal(configurations, &configMap)
			if err != nil {
				return status.Error(codes.Internal, "failed to read configs")
			}
		}

		exportURL := sc.ExportURL
		if exportURL == "" {
			exportURL = pluginExportURL
		}
		config := &scripts.Config{
			OtelEndpointConfig: &scripts.OtelEndpointConfig{
				URL:      exportURL,
				Headers:  configMap,
				Insecure: insecureTLS,
			},
		}

		mConfig, err := yaml.Marshal(&config)
		if err != nil {
			return status.Error(codes.Internal, "failed to marshal configs")
		}

		_, err = s.cronScriptClient.UpdateScript(ctx, &cronscriptpb.UpdateScriptRequest{
			ScriptId: utils.ProtoFromUUID(sc.ScriptID),
			Configs:  &types.StringValue{Value: string(mConfig)},
			OrgID:    utils.ProtoFromUUID(orgID),
		})
		if err != nil {
			log.WithError(err).Error("Failed to update cron script")
			continue
		}
	}

	return nil
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

	txn, err := s.db.Beginx()
	if err != nil {
		return nil, err
	}
	defer txn.Rollback()

	// Fetch current configs.
	query := `SELECT version, PGP_SYM_DECRYPT(configurations, $1::text), PGP_SYM_DECRYPT(custom_export_url, $1::text), insecure_tls FROM org_data_retention_plugins WHERE org_id=$2 AND plugin_id=$3`
	rows, err := txn.Queryx(query, s.dbKey, orgID, req.PluginID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch plugin")
	}

	var origConfig []byte
	var origVersion string
	var customExportURL *string
	var insecureTLS bool
	enabled := false
	defer rows.Close()
	if rows.Next() {
		enabled = true
		err := rows.Scan(&origVersion, &origConfig, &customExportURL, &insecureTLS)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read configs")
		}
	}
	rows.Close()

	if version == "" {
		version = origVersion
	}

	query = `SELECT allow_custom_export_url, allow_insecure_tls FROM data_retention_plugin_releases WHERE plugin_id=$1 AND version=$2`
	rows, err = txn.Queryx(query, req.PluginID, version)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch plugin")
	}
	defer rows.Close()
	var allowCustomExportURL bool
	var allowInsecureTLS bool
	if rows.Next() {
		err := rows.Scan(&allowCustomExportURL, &allowInsecureTLS)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read plugin")
		}
	}
	rows.Close()

	if req.CustomExportUrl != nil && allowCustomExportURL {
		customExportURL = &req.CustomExportUrl.Value
	} else if !allowCustomExportURL {
		customExportURL = nil
	}

	if req.InsecureTLS != nil && allowInsecureTLS {
		insecureTLS = req.InsecureTLS.Value
	} else if !allowInsecureTLS {
		insecureTLS = false
	}

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	if !enabled && req.Enabled != nil && req.Enabled.Value { // Plugin was just enabled, we should create it.
		disablePresets := false
		if req.DisablePresets != nil {
			disablePresets = req.DisablePresets.Value
		}

		err = s.enableOrgRetention(ctx, txn, orgID, req.PluginID, version, configurations, customExportURL, insecureTLS, disablePresets)
		if err != nil {
			return nil, err
		}

		log.WithField("plugin_id", req.PluginID).WithField("version", req.Version).WithField("org", orgID).Info("Plugin enabled")

		// Track enable event.
		events.Client().Enqueue(&analytics.Track{
			UserId: orgID.String(),
			Event:  events.PluginEnabled,
			Properties: analytics.NewProperties().
				Set("plugin_id", req.PluginID).
				Set("version", req.Version),
		})

		return &pluginpb.UpdateOrgRetentionPluginConfigResponse{}, txn.Commit()
	} else if enabled && req.Enabled != nil && !req.Enabled.Value { // Plugin was disabled, we should delete it.
		err = s.disableOrgRetention(ctx, txn, orgID, req.PluginID)
		if err != nil {
			return nil, err
		}
		log.WithField("plugin_id", req.PluginID).WithField("org", orgID).Info("Plugin disabled")

		// Track disable event.
		events.Client().Enqueue(&analytics.Track{
			UserId: orgID.String(),
			Event:  events.PluginDisabled,
			Properties: analytics.NewProperties().
				Set("plugin_id", req.PluginID),
		})
		return &pluginpb.UpdateOrgRetentionPluginConfigResponse{}, txn.Commit()
	} else if !enabled && req.Enabled != nil && !req.Enabled.Value {
		// This is already disabled.
		return &pluginpb.UpdateOrgRetentionPluginConfigResponse{}, nil
	}

	if configurations == nil {
		configurations = origConfig
	}

	err = s.updateOrgRetentionConfigs(ctx, txn, orgID, req.PluginID, version, configurations, customExportURL, insecureTLS)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to update configs")
	}

	if origVersion != version { // The user is updating the plugin, and some of the preset scripts have likely changed.
		err = s.updatePresetScripts(ctx, txn, orgID, req.PluginID, version, configurations, customExportURL, false)
		if err != nil {
			return nil, err
		}
	}

	err = txn.Commit()
	if err != nil {
		return nil, err
	}

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
	query := `SELECT r.script_id, r.script_name, r.description, r.is_preset, r.plugin_id from plugin_retention_scripts r, org_data_retention_plugins o WHERE r.org_id=$1 AND r.org_id = o.org_id AND r.plugin_id = o.plugin_id`
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

	if len(scriptIDs) == 0 {
		return &pluginpb.GetRetentionScriptsResponse{Scripts: make([]*pluginpb.RetentionScript, 0)}, nil
	}

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	cronScriptsResp, err := s.cronScriptClient.GetScripts(ctx, &cronscriptpb.GetScriptsRequest{IDs: scriptIDs, OrgID: utils.ProtoFromUUID(orgID)})
	if err != nil {
		log.WithField("orgID", orgID.String()).WithError(err).Error("Failed to fetch cron scripts for retention scripts for org")
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

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	cronScriptResp, err := s.cronScriptClient.GetScript(ctx, &cronscriptpb.GetScriptRequest{ID: req.ScriptID, OrgID: utils.ProtoFromUUID(orgID)})
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

func (s *Server) createRetentionScript(ctx context.Context, txn *sqlx.Tx, orgID uuid.UUID, pluginID string, rs *RetentionScript, contents string, clusterIDs []*uuidpb.UUID, frequencyS int64, disabled bool) (*uuidpb.UUID, error) {
	pluginExportURL, configMap, insecureTLS, err := s.getPluginConfigs(txn, orgID, pluginID)
	if err != nil {
		return nil, err
	}

	exportURL := pluginExportURL
	if rs.ExportURL != "" {
		exportURL = rs.ExportURL
	}

	configYAML, err := scriptConfigToYAML(configMap, exportURL, insecureTLS)
	if err != nil {
		return nil, err
	}
	cronScriptResp, err := s.cronScriptClient.CreateScript(ctx, &cronscriptpb.CreateScriptRequest{
		Script:     contents,
		ClusterIDs: clusterIDs,
		Configs:    configYAML,
		FrequencyS: frequencyS,
		Disabled:   disabled,
		OrgID:      utils.ProtoFromUUID(orgID),
	})
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to create cron script")
	}

	scriptID := cronScriptResp.ID

	query := `INSERT INTO plugin_retention_scripts (org_id, plugin_id, script_id, script_name, description, export_url, is_preset) VALUES ($1, $2, $3, $4, $5, PGP_SYM_ENCRYPT($6, $7), $8)`
	_, err = txn.Exec(query, orgID, pluginID, utils.UUIDFromProtoOrNil(scriptID), rs.ScriptName, rs.Description, rs.ExportURL, s.dbKey, rs.IsPreset)
	if err == nil {
		return scriptID, nil
	}

	// Create failed, make sure we clean up the cron script.
	_, delErr := s.cronScriptClient.DeleteScript(ctx, &cronscriptpb.DeleteScriptRequest{
		ID:    scriptID,
		OrgID: utils.ProtoFromUUID(orgID),
	})
	if delErr != nil {
		log.WithError(delErr).Error("Failed to delete underlying cron script")
		return nil, err
	}

	return nil, err
}

// CreateRetentionScript creates a script that is used for long-term data retention.
func (s *Server) CreateRetentionScript(ctx context.Context, req *pluginpb.CreateRetentionScriptRequest) (*pluginpb.CreateRetentionScriptResponse, error) {
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	txn, err := s.db.Beginx()
	if err != nil {
		return nil, err
	}
	defer txn.Rollback()

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	id, err := s.createRetentionScript(ctx, txn, orgID, req.Script.Script.PluginId, &RetentionScript{
		ScriptName:  req.Script.Script.ScriptName,
		Description: req.Script.Script.Description,
		IsPreset:    req.Script.Script.IsPreset,
		ExportURL:   req.Script.ExportURL,
	}, req.Script.Contents, req.Script.Script.ClusterIDs, req.Script.Script.FrequencyS, !req.Script.Script.Enabled)
	if err != nil {
		return nil, err
	}

	err = txn.Commit()
	if err != nil {
		return nil, err
	}

	events.Client().Enqueue(&analytics.Track{
		UserId: orgID.String(),
		Event:  events.PluginRetentionScriptCreated,
		Properties: analytics.NewProperties().
			Set("plugin_id", req.Script.Script.PluginId).
			Set("script_name", req.Script.Script.ScriptName).
			Set("script_id", utils.UUIDFromProtoOrNil(id).String()).
			Set("frequency_s", req.Script.Script.FrequencyS),
	})

	return &pluginpb.CreateRetentionScriptResponse{
		ID: id,
	}, nil
}

func (s *Server) getPluginConfigs(txn *sqlx.Tx, orgID uuid.UUID, pluginID string) (string, map[string]string, bool, error) {
	query := `SELECT PGP_SYM_DECRYPT(o.configurations, $1::text), r.default_export_url, PGP_SYM_DECRYPT(o.custom_export_url, $1::text), insecure_tls FROM org_data_retention_plugins o, data_retention_plugin_releases r WHERE org_id=$2 AND r.plugin_id=$3 AND o.plugin_id=r.plugin_id AND r.version = o.version`
	rows, err := txn.Queryx(query, s.dbKey, orgID, pluginID)
	if err != nil {
		return "", nil, false, status.Errorf(codes.Internal, "failed to fetch plugin")
	}
	defer rows.Close()

	if !rows.Next() {
		return "", nil, false, status.Error(codes.NotFound, "plugin is not enabled")
	}

	var configurationJSON []byte
	var configMap map[string]string
	var pluginExportURL string
	var customExportURL *string
	var insecureTLS bool
	err = rows.Scan(&configurationJSON, &pluginExportURL, &customExportURL, &insecureTLS)
	if err != nil {
		return "", nil, false, status.Error(codes.Internal, "failed to read configs")
	}
	if len(configurationJSON) > 0 {
		err = json.Unmarshal(configurationJSON, &configMap)
		if err != nil {
			return "", nil, false, status.Error(codes.Internal, "failed to read configs")
		}
	}

	if customExportURL != nil {
		pluginExportURL = *customExportURL
	}

	return pluginExportURL, configMap, insecureTLS, nil
}

func scriptConfigToYAML(configMap map[string]string, exportURL string, insecureTLS bool) (string, error) {
	config := &scripts.Config{
		OtelEndpointConfig: &scripts.OtelEndpointConfig{
			URL:      exportURL,
			Headers:  configMap,
			Insecure: insecureTLS,
		},
	}

	mConfig, err := yaml.Marshal(&config)
	if err != nil {
		return "", status.Error(codes.Internal, "failed to marshal configs")
	}
	return string(mConfig), nil
}

// UpdateRetentionScript updates a script used for long-term data retention.
func (s *Server) UpdateRetentionScript(ctx context.Context, req *pluginpb.UpdateRetentionScriptRequest) (*pluginpb.UpdateRetentionScriptResponse, error) {
	txn, err := s.db.Beginx()
	if err != nil {
		return nil, err
	}
	defer txn.Rollback()

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	// Fetch existing script.
	query := `SELECT org_id, script_name, description, PGP_SYM_DECRYPT(export_url, $1::text) as export_url, plugin_id from plugin_retention_scripts WHERE script_id=$2`
	scriptID := utils.UUIDFromProtoOrNil(req.ScriptID)
	rows, err := txn.Queryx(query, s.dbKey, scriptID)
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
	rows.Close()

	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthenticated")
	}
	claimsOrgIDstr := sCtx.Claims.GetUserClaims().OrgID
	if script.OrgID.String() != claimsOrgIDstr {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthorized")
	}

	// Fetch config + headers from plugin info.
	pluginExportURL, configMap, insecureTLS, err := s.getPluginConfigs(txn, script.OrgID, script.PluginID)
	if err != nil {
		return nil, err
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

	// Update retention scripts with new info.
	query = `UPDATE plugin_retention_scripts SET script_name = $1, export_url = PGP_SYM_ENCRYPT($2, $3), description = $4 WHERE script_id = $5`
	_, err = txn.Exec(query, scriptName, exportURL, s.dbKey, description, scriptID)

	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to update retention script")
	}
	err = txn.Commit()
	if err != nil {
		return nil, err
	}

	// Create updated config with new export URL.
	configExportURL := exportURL
	if exportURL == "" {
		configExportURL = pluginExportURL
	}
	configYAML, err := scriptConfigToYAML(configMap, configExportURL, insecureTLS)
	if err != nil {
		return nil, err
	}

	// Update cron script.
	_, err = s.cronScriptClient.UpdateScript(ctx, &cronscriptpb.UpdateScriptRequest{
		Script:     req.Contents,
		ClusterIDs: &cronscriptpb.ClusterIDs{Value: req.ClusterIDs},
		Enabled:    req.Enabled,
		FrequencyS: req.FrequencyS,
		ScriptId:   req.ScriptID,
		Configs:    &types.StringValue{Value: configYAML},
		OrgID:      utils.ProtoFromUUIDStrOrNil(claimsOrgIDstr),
	})
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to update cron script")
	}

	// Send event to track script updates.
	changedScriptName := req.ScriptName != nil
	changedDescription := req.Description != nil
	changedEnabled := req.Enabled != nil
	enabled := false
	if changedEnabled {
		enabled = req.Enabled.Value
	}
	changedFrequency := req.FrequencyS != nil
	freq := int64(0)
	if changedFrequency {
		freq = req.FrequencyS.Value
	}
	changedScript := req.Contents != nil
	changedExportURL := req.ExportUrl != nil

	clusterIDStrs := make([]string, len(req.ClusterIDs))
	for i, c := range req.ClusterIDs {
		clusterIDStrs[i] = utils.UUIDFromProtoOrNil(c).String()
	}

	events.Client().Enqueue(&analytics.Track{
		UserId: claimsOrgIDstr,
		Event:  events.PluginRetentionScriptUpdated,
		Properties: analytics.NewProperties().
			Set("script_id", scriptID).
			Set("plugin_id", script.PluginID).
			Set("cluster_ids", clusterIDStrs).
			Set("changed_script_name", changedScriptName).
			Set("changed_description", changedDescription).
			Set("changed_enabled", changedEnabled).
			Set("changed_script", changedScript).
			Set("changed_export_url", changedExportURL).
			Set("enabled", enabled).
			Set("frequency_s", freq),
	})

	return &pluginpb.UpdateRetentionScriptResponse{}, nil
}

// DeleteRetentionScript creates a script that is used for long-term data retention.
func (s *Server) DeleteRetentionScript(ctx context.Context, req *pluginpb.DeleteRetentionScriptRequest) (*pluginpb.DeleteRetentionScriptResponse, error) {
	txn, err := s.db.Beginx()
	if err != nil {
		return nil, err
	}
	defer txn.Rollback()

	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	scriptID := utils.UUIDFromProtoOrNil(req.ID)

	query := `SELECT plugin_id from plugin_retention_scripts WHERE script_id=$1`
	selectRows, err := s.db.Queryx(query, scriptID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch script")
	}

	defer selectRows.Close()

	if !selectRows.Next() {
		return nil, status.Error(codes.NotFound, "script not found")
	}

	var pluginID string
	err = selectRows.Scan(&pluginID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to read script")
	}

	query = `DELETE FROM plugin_retention_scripts WHERE org_id=$1 AND script_id=$2 AND NOT is_preset`
	resp, err := txn.Exec(query, orgID, scriptID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to delete scripts")
	}

	rowsDel, err := resp.RowsAffected()
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to delete scripts")
	}
	if rowsDel == 0 {
		return nil, status.Errorf(codes.Internal, "No script to delete")
	}

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	_, err = s.cronScriptClient.DeleteScript(ctx, &cronscriptpb.DeleteScriptRequest{
		ID:    req.ID,
		OrgID: utils.ProtoFromUUID(orgID),
	})
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to delete cron script")
	}

	err = txn.Commit()
	if err != nil {
		return nil, err
	}

	events.Client().Enqueue(&analytics.Track{
		UserId: orgID.String(),
		Event:  events.PluginRetentionScriptDeleted,
		Properties: analytics.NewProperties().
			Set("script_id", scriptID.String()).
			Set("plugin_id", pluginID),
	})

	return &pluginpb.DeleteRetentionScriptResponse{}, nil
}
