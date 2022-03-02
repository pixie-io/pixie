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
	"fmt"
	"sync"

	"github.com/jmoiron/sqlx"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/plugin/pluginpb"
)

// Server is a bridge implementation of the pluginService.
type Server struct {
	db    *sqlx.DB
	dbKey string

	done chan struct{}
	once sync.Once
}

// New creates a new server.
func New(db *sqlx.DB, dbKey string) *Server {
	return &Server{
		db:    db,
		dbKey: dbKey,
		done:  make(chan struct{}),
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
