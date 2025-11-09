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

package main

import (
	"context"
	"fmt"
	"os"
	"time"

	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/vizier/services/adaptive_export/internal/config"
	"px.dev/pixie/src/vizier/services/adaptive_export/internal/pixie"
	"px.dev/pixie/src/vizier/services/adaptive_export/internal/script"
)

const (
	defaultRetries   = 100
	defaultSleepTime = 15 * time.Second
)

func main() {
	ctx := context.Background()

	log.Info("Starting the setup of the ClickHouse Pixie plugin")
	cfg, err := config.GetConfig()
	if err != nil {
		log.Error(err)
		os.Exit(1)
	}

	clusterId := cfg.Pixie().ClusterID()
	clusterName := cfg.Worker().ClusterName()

	log.Infof("Setting up Pixie plugin for cluster-id %s", clusterId)
	client, err := setupPixie(ctx, cfg.Pixie(), defaultRetries, defaultSleepTime)
	if err != nil {
		log.WithError(err).Fatal("setting up Pixie client failed")
	}

	log.Info("Checking the current ClickHouse plugin configuration")
	plugin, err := client.GetClickHousePlugin()
	if err != nil {
		log.WithError(err).Fatal("getting data retention plugins failed")
	}

	enablePlugin := true
	if plugin.RetentionEnabled {
		enablePlugin = false
		config, err := client.GetClickHousePluginConfig()
		if err != nil {
			log.WithError(err).Fatal("getting ClickHouse plugin config failed")
		}
		if config.ExportUrl != cfg.ClickHouse().DSN() {
			log.Info("ClickHouse plugin is configured with different DSN... Overwriting")
			enablePlugin = true
		}
	}

	if enablePlugin {
		log.Info("Enabling ClickHouse plugin")
		err := client.EnableClickHousePlugin(&pixie.ClickHousePluginConfig{
			ExportUrl: cfg.ClickHouse().DSN(),
		}, plugin.LatestVersion)
		if err != nil {
			log.WithError(err).Fatal("failed to enabled ClickHouse plugin")
		}
	}

	log.Info("Setting up the data retention scripts")

	log.Info("Getting preset script from the Pixie plugin")
	defsFromPixie, err := client.GetPresetScripts()
	if err != nil {
		log.WithError(err).Fatal("failed to get preset scripts")
	}

	definitions := defsFromPixie

	log.Infof("Getting current scripts for cluster")
	currentScripts, err := client.GetClusterScripts(clusterId, clusterName)
	if err != nil {
		log.WithError(err).Fatal("failed to get data retention scripts")
	}

	actions := script.GetActions(definitions, currentScripts, script.ScriptConfig{
		ClusterName:     clusterName,
		ClusterId:       clusterId,
		CollectInterval: cfg.Worker().CollectInterval(),
	})

	var errs []error

	for _, s := range actions.ToDelete {
		log.Infof("Deleting script %s", s.Name)
		err := client.DeleteDataRetentionScript(s.ScriptId)
		if err != nil {
			errs = append(errs, err)
		}
	}

	for _, s := range actions.ToUpdate {
		log.Infof("Updating script %s", s.Name)
		err := client.UpdateDataRetentionScript(clusterId, s.ScriptId, s.Name, s.Description, s.FrequencyS, s.Script)
		if err != nil {
			errs = append(errs, err)
		}
	}

	for _, s := range actions.ToCreate {
		log.Infof("Creating script %s", s.Name)
		err := client.AddDataRetentionScript(clusterId, s.Name, s.Description, s.FrequencyS, s.Script)
		if err != nil {
			errs = append(errs, err)
		}
	}

	if len(errs) > 0 {
		log.Fatalf("errors while setting up data retention scripts: %v", errs)
	}

	log.Info("All done! The ClickHouse plugin is now configured.")
	os.Exit(0)
}

func setupPixie(ctx context.Context, cfg config.Pixie, tries int, sleepTime time.Duration) (*pixie.Client, error) {
	apiKey := cfg.APIKey()
	host := cfg.Host()
	log.Infof("setupPixie: API Key length=%d, Host=%s", len(apiKey), host)

	for tries > 0 {
		client, err := pixie.NewClient(ctx, apiKey, host)
		if err == nil {
			return client, nil
		}
		tries -= 1
		log.WithError(err).Warning("error creating Pixie API client")
		time.Sleep(sleepTime)
	}
	return nil, fmt.Errorf("exceeded maximum number of retries")
}
