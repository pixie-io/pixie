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

package main

import (
	"archive/tar"
	"compress/gzip"
	"context"
	"database/sql"
	"fmt"
	"io"
	"net/http"
	"path/filepath"

	"github.com/blang/semver"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/jmoiron/sqlx"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"
	"gopkg.in/yaml.v2"

	"px.dev/pixie/src/cloud/plugin/controllers"
	"px.dev/pixie/src/cloud/plugin/pluginpb"
	"px.dev/pixie/src/cloud/plugin/schema"
	"px.dev/pixie/src/cloud/shared/pgmigrate"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/pg"
	srvutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

const githubArchiveTmpl = "https://api.github.com/repos/%s/tarball"

// NOTE: After changing this file, remember to push a generated image by running
// bazel run //src/cloud/plugin/load_db:push_plugin_db_updater_image
func init() {
	pflag.String("plugin_repo", "pixie-io/pixie-plugin", "The name of the plugin repo.")
	pflag.String("plugin_service", "plugin-service.plc.svc.cluster.local:50600", "The plugin service url (load balancer/list is ok)")
	pflag.String("domain_name", "dev.withpixie.dev", "The domain name of Pixie Cloud")
}

func initDB() *sqlx.DB {
	db := pg.MustConnectDefaultPostgresDB()
	err := pgmigrate.PerformMigrationsUsingBindata(db, "plugin_service_migrations",
		bindata.Resource(schema.AssetNames(), schema.Asset))
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}
	return db
}

func initPluginServiceClients() (pluginpb.PluginServiceClient, pluginpb.DataRetentionPluginServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, nil, err
	}

	pluginChan, err := grpc.Dial(viper.GetString("plugin_service"), dialOpts...)
	if err != nil {
		return nil, nil, err
	}

	return pluginpb.NewPluginServiceClient(pluginChan), pluginpb.NewDataRetentionPluginServiceClient(pluginChan), nil
}

type configSet struct {
	plugin    *controllers.Plugin
	retention *controllers.RetentionPlugin
}

var configsByPath = make(map[string]*configSet)

func loadPlugins(db *sqlx.DB) {
	pluginRepo := viper.GetString("plugin_repo")
	if pluginRepo == "" {
		log.Fatal("Must specify --plugin_repo")
	}

	client := http.Client{}
	req, err := http.NewRequest("GET", fmt.Sprintf(githubArchiveTmpl, pluginRepo), nil)
	if err != nil {
		log.WithError(err).Fatal("Failed to create req")
	}

	apiKey := viper.GetString("gh_api_key")
	if apiKey != "" {
		req.Header = http.Header{
			"Authorization": {fmt.Sprintf("token %s", apiKey)},
		}
	}

	resp, err := client.Do(req)
	if err != nil {
		log.WithError(err).Fatal("Failed to fetch plugin repo")
	}
	defer resp.Body.Close()
	archive, err := gzip.NewReader(resp.Body)
	if err != nil {
		log.WithError(err).Fatal("Failed to uncompress plugin repo")
	}

	data := tar.NewReader(archive)

	for {
		header, err := data.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			log.WithError(err).Fatal("Failed to untar plugin repo")
		}

		switch header.Typeflag {
		case tar.TypeReg:
			processFile(header.Name, data, db)
		default:
			// noop
		}
	}
}

func processFile(path string, plugin io.Reader, db *sqlx.DB) {
	base, configType := filepath.Split(path)

	if _, ok := configsByPath[base]; !ok {
		configsByPath[base] = &configSet{}
	}
	configSet := configsByPath[base]

	switch configType {
	case "plugin.yaml":
		log.Infof("Processing plugin %s", path)
		d := yaml.NewDecoder(plugin)
		config := &controllers.Plugin{}
		err := d.Decode(config)
		if err != nil {
			log.WithError(err).Warnf("Failed to decode plugin: %s", path)
		}
		configSet.plugin = config
	case "retention.yaml":
		log.Infof("Processing retention %s", path)
		d := yaml.NewDecoder(plugin)
		config := &controllers.RetentionPlugin{}
		err := d.Decode(config)
		if err != nil {
			log.WithError(err).Warnf("Failed to decode plugin: %s", path)
		}
		configSet.retention = config
	default:
		log.Infof("Ignoring %s", path)
	}

	if configSet.plugin != nil && (configSet.retention != nil || !configSet.plugin.DataRetentionEnabled) {
		addConfigs(configSet.plugin, configSet.retention, db)
		delete(configsByPath, base)
	}
}

func addConfigs(plugin *controllers.Plugin, retention *controllers.RetentionPlugin, db *sqlx.DB) {
	if plugin == nil {
		return
	}

	query := `SELECT EXISTS(SELECT 1 FROM plugin_releases WHERE id=$1 AND version=$2)`

	var exists bool
	err := db.QueryRow(query, plugin.ID, plugin.Version).Scan(&exists)
	if err == sql.ErrNoRows || !exists {
		insertPlugin := `INSERT INTO plugin_releases
		(name, id, description, logo, version, data_retention_enabled)
		VALUES ($1, $2, $3, $4, $5, $6)`
		_, err = db.Exec(insertPlugin,
			plugin.Name, plugin.ID, plugin.Description,
			plugin.Logo, plugin.Version, plugin.DataRetentionEnabled,
		)
		if err != nil {
			log.WithError(err).Errorf("Failed to insert plugin release for Plugin ID %s", plugin.ID)
			return
		}
	}

	if retention == nil || !plugin.DataRetentionEnabled {
		return
	}

	query = `SELECT EXISTS(SELECT 1 FROM data_retention_plugin_releases WHERE id=$1 AND version=$2)`
	err = db.QueryRow(query, plugin.ID, plugin.Version).Scan(&exists)
	if err == sql.ErrNoRows || !exists {
		insertRetention := `INSERT INTO data_retention_plugin_releases
			(plugin_id, version, configurations, documentation_url,
			default_export_url, allow_custom_export_url, preset_scripts, allow_insecure_tls)
		VALUES ($1, $2, $3, $4, $5, $6, $7, $8)`
		_, err = db.Exec(insertRetention,
			plugin.ID, plugin.Version, retention.Configurations, retention.DocumentationURL,
			retention.DefaultExportURL, retention.AllowCustomExportURL, retention.PresetScripts, retention.AllowInsecureTLS,
		)
		if err != nil {
			log.WithError(err).Errorf("Failed to insert data retention plugin release for Plugin ID %s", plugin.ID)
			return
		}
	}
}

type pluginRelease struct {
	ID      string `db:"plugin_id"`
	Version string `db:"version"`
}

func getServiceCredentials(signingKey string) (string, error) {
	claims := srvutils.GenerateJWTForService("PluginLoader", viper.GetString("domain_name"))
	return srvutils.SignJWTClaims(claims, signingKey)
}

// UpdatePlugins loops through all enabled plugins and auto-updates them to the most recent version.
func UpdatePlugins(db *sqlx.DB, pClient pluginpb.DataRetentionPluginServiceClient) {
	txn, err := db.Beginx()
	if err != nil {
		log.WithError(err).Fatal("Failed to start db transaction")
	}
	defer txn.Rollback()

	// Get the latest version for each available plugin major version.
	latestVersions := make(map[string]map[uint64]semver.Version)
	query := `SELECT plugin_id, version FROM data_retention_plugin_releases`
	rows, err := txn.Queryx(query)
	if err != nil {
		log.WithError(err).Fatal("Failed to fetch plugin releases")
	}
	defer rows.Close()
	for rows.Next() {
		var p pluginRelease
		err = rows.StructScan(&p)
		if err != nil {
			log.WithError(err).Error("Failed to read plugin release")
			continue
		}
		currVers := semver.MustParse(p.Version)
		// Get major version.
		if _, ok := latestVersions[p.ID]; !ok {
			latestVersions[p.ID] = make(map[uint64]semver.Version)
		}

		if v, ok := latestVersions[p.ID][currVers.Major]; ok {
			if v.Compare(currVers) < 0 {
				latestVersions[p.ID][currVers.Major] = currVers
			}
		} else {
			latestVersions[p.ID][currVers.Major] = currVers
		}
	}

	// Fetch all enabled plugins and current versions.
	query = `SELECT plugin_id, version, org_id FROM org_data_retention_plugins`
	rows2, err := txn.Queryx(query)
	if err != nil {
		log.WithError(err).Fatal("Failed to fetch org retention plugins")
	}
	for rows2.Next() {
		var pluginID string
		var orgID uuid.UUID
		var version string

		err = rows2.Scan(&pluginID, &version, &orgID)
		if err != nil {
			log.WithError(err).Error("Failed to read org retention plugin")
			continue
		}
		currVer := semver.MustParse(version)

		if _, ok := latestVersions[pluginID]; !ok {
			continue
		}

		// If version is out-of-date, make request to plugin server to update.
		if v, ok := latestVersions[pluginID][currVer.Major]; ok {
			if currVer.Compare(v) >= 0 {
				continue
			}
			serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
			if err != nil {
				log.WithError(err).Fatal("Failed to generate service credentials")
			}

			ctxWithCreds := metadata.AppendToOutgoingContext(context.Background(), "authorization",
				fmt.Sprintf("bearer %s", serviceAuthToken))

			updateReq := &pluginpb.UpdateOrgRetentionPluginConfigRequest{
				PluginID: pluginID,
				OrgID:    utils.ProtoFromUUID(orgID),
				Version:  &types.StringValue{Value: v.String()},
			}

			_, err = pClient.UpdateOrgRetentionPluginConfig(ctxWithCreds, updateReq)
			if err != nil {
				log.WithError(err).Error("Failed to update retention plugin")
			}
		}
	}
}

func main() {
	log.Info("Starting load_plugins...")
	pflag.Parse()

	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()

	db := initDB()
	_, retentionPluginClient, err := initPluginServiceClients()
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to plugin service")
	}
	loadPlugins(db)

	// Auto-update any plugins.
	UpdatePlugins(db, retentionPluginClient)
}
