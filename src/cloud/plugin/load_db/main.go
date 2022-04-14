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
	"database/sql"
	"fmt"
	"io"
	"net/http"
	"path/filepath"

	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/jmoiron/sqlx"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"gopkg.in/yaml.v2"

	"px.dev/pixie/src/cloud/plugin/controllers"
	"px.dev/pixie/src/cloud/plugin/schema"
	"px.dev/pixie/src/cloud/shared/pgmigrate"
	"px.dev/pixie/src/shared/services/pg"
)

const githubArchiveTmpl = "https://api.github.com/repos/%s/tarball"

// NOTE: After changing this file, remember to push a generated image by running
// bazel run //src/cloud/plugin/load_db:push_plugin_db_updater_image
func init() {
	pflag.String("plugin_repo", "pixie-io/pixie-plugin", "The name of the plugin repo.")
}

func initDBAndLoadPlugins() {
	db := pg.MustConnectDefaultPostgresDB()
	err := pgmigrate.PerformMigrationsUsingBindata(db, "plugin_service_migrations",
		bindata.Resource(schema.AssetNames(), schema.Asset))
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}
	loadPlugins(db)
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

	resp, err := http.Get(fmt.Sprintf(githubArchiveTmpl, pluginRepo))
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

func main() {
	log.Info("Starting load_plugins...")
	pflag.Parse()

	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	initDBAndLoadPlugins()
}
