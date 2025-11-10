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
	"os/signal"
	"syscall"
	"time"

	log "github.com/sirupsen/logrus"
	"px.dev/pixie/src/api/go/pxapi"

	"px.dev/pixie/src/vizier/services/adaptive_export/internal/config"
	"px.dev/pixie/src/vizier/services/adaptive_export/internal/pixie"
	"px.dev/pixie/src/vizier/services/adaptive_export/internal/pxl"
	"px.dev/pixie/src/vizier/services/adaptive_export/internal/script"
)

const (
	defaultRetries         = 100
	defaultSleepTime       = 15 * time.Second
	schemaCreationInterval = 2 * time.Minute
	setupTimeout           = 30 * time.Second
	scriptExecutionTimeout = 60 * time.Second
)

const (
	// TODO(ddelnano): Clickhouse configuration should come from plugin config.
	schemaCreationScript = `
import px
px.display(px.CreateClickHouseSchemas(
  host="hyperdx-hdx-oss-v2-clickhouse.click.svc.cluster.local",
  port=9000,
  username="otelcollector",
  password="otelcollectorpass",
  database="default"
))
`
	detectionScript = `
import px

df = px.DataFrame('kubescape_logs', clickhouse_dsn='otelcollector:otelcollectorpass@hyperdx-hdx-oss-v2-clickhouse.click.svc.cluster.local:9000/default', start_time='-%ds')
df.alert = df.message
df.namespace = px.pluck(df.RuntimeK8sDetails, "podNamespace")
df.podName = px.pluck(df.RuntimeK8sDetails, "podName")
df.time_ = px.int64_to_time(df.event_time * 1000000000)
df = df[['time_', 'alert', 'namespace', 'podName']]
px.display(df)
`
)

func main() {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	log.Info("Starting the ClickHouse Adaptive Export service")
	cfg, err := config.GetConfig()
	if err != nil {
		log.WithError(err).Fatal("failed to load configuration")
	}

	clusterId := cfg.Pixie().ClusterID()
	clusterName := cfg.Worker().ClusterName()

	// Setup Pixie Plugin API client
	log.Infof("Setting up Pixie plugin API client for cluster-id %s", clusterId)
	pluginClient, err := setupPixie(ctx, cfg.Pixie(), defaultRetries, defaultSleepTime)
	if err != nil {
		log.WithError(err).Fatal("setting up Pixie plugin client failed")
	}

	// Setup Pixie pxapi client for executing PxL scripts
	log.Info("Setting up Pixie pxapi client")
	// Use parent context - client stores this and uses it for all subsequent operations
	pxClient, err := pxapi.NewClient(ctx, pxapi.WithAPIKey(cfg.Pixie().APIKey()), pxapi.WithCloudAddr(cfg.Pixie().Host()))
	if err != nil {
		log.WithError(err).Fatal("failed to create pxapi client")
	}

	// Start schema creation background task
	go runSchemaCreationTask(ctx, pxClient, clusterId)

	// Start detection script that monitors for when to enable persistence
	go runDetectionTask(ctx, pxClient, pluginClient, cfg, clusterId, clusterName)

	// Wait for signal to shutdown
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
	<-sigCh

	log.Info("Shutting down adaptive export service")
	cancel()
	time.Sleep(1 * time.Second)
}

func runSchemaCreationTask(ctx context.Context, client *pxapi.Client, clusterID string) {
	ticker := time.NewTicker(schemaCreationInterval)
	defer ticker.Stop()

	// Run immediately on startup
	log.Info("Running schema creation script")
	execCtx, cancel := context.WithTimeout(ctx, scriptExecutionTimeout)
	if _, err := pxl.ExecuteScript(execCtx, client, clusterID, schemaCreationScript); err != nil {
		log.WithError(err).Error("failed to execute schema creation script")
	} else {
		log.Info("Schema creation script completed successfully")
	}
	cancel()

	for {
		select {
		case <-ctx.Done():
			log.Info("Schema creation task shutting down")
			return
		case <-ticker.C:
			log.Info("Running schema creation script")
			execCtx, cancel := context.WithTimeout(ctx, scriptExecutionTimeout)
			if _, err := pxl.ExecuteScript(execCtx, client, clusterID, schemaCreationScript); err != nil {
				log.WithError(err).Error("failed to execute schema creation script")
			} else {
				log.Info("Schema creation script completed successfully")
			}
			cancel()
		}
	}
}

func runDetectionTask(ctx context.Context, pxClient *pxapi.Client, pluginClient *pixie.Client, cfg config.Config, clusterID string, clusterName string) {
	detectionInterval := time.Duration(cfg.Worker().DetectionInterval()) * time.Second
	detectionLookback := cfg.Worker().DetectionLookback()

	ticker := time.NewTicker(detectionInterval)
	defer ticker.Stop()

	pluginEnabled := false

	for {
		select {
		case <-ctx.Done():
			log.Info("Detection task shutting down")
			return
		case <-ticker.C:
			log.Info("Running detection script")
			// Run detection script with lookback period
			detectionPxl := fmt.Sprintf(detectionScript, detectionLookback)
			execCtx, cancel := context.WithTimeout(ctx, scriptExecutionTimeout)
			recordCount, err := pxl.ExecuteScript(execCtx, pxClient, clusterID, detectionPxl)
			cancel()

			if err != nil {
				log.WithError(err).Error("failed to execute detection script")
				continue
			}

			log.Debugf("Detection script returned %d records", recordCount)

			// If we have records and plugin is not enabled, enable it
			if recordCount > 0 && !pluginEnabled {
				log.Info("Detection script returned records - enabling forensic export")
				pluginCtx, pluginCancel := context.WithTimeout(ctx, 2*time.Minute)
				if err := enableClickHousePlugin(pluginCtx, pluginClient, cfg, clusterID, clusterName); err != nil {
					log.WithError(err).Error("failed to enable forensic export")
				} else {
					pluginEnabled = true
					log.Info("Forensic export enabled successfully")
				}
				pluginCancel()
			} else if recordCount > 0 && pluginEnabled {
				log.Info("Detection script returned records but forensic export already enabled, no action taken")
			}
		}
	}
}

func enableClickHousePlugin(ctx context.Context, client *pixie.Client, cfg config.Config, clusterID string, clusterName string) error {
	log.Info("Checking the current ClickHouse plugin configuration")
	plugin, err := client.GetClickHousePlugin()
	if err != nil {
		return fmt.Errorf("getting data retention plugins failed: %w", err)
	}

	enablePlugin := true
	if plugin.RetentionEnabled {
		enablePlugin = false
		config, err := client.GetClickHousePluginConfig()
		if err != nil {
			return fmt.Errorf("getting ClickHouse plugin config failed: %w", err)
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
			return fmt.Errorf("failed to enable ClickHouse plugin: %w", err)
		}
	}

	log.Info("Setting up the data retention scripts")

	log.Info("Getting preset script from the Pixie plugin")
	defsFromPixie, err := client.GetPresetScripts()
	if err != nil {
		return fmt.Errorf("failed to get preset scripts: %w", err)
	}

	definitions := defsFromPixie

	log.Infof("Getting current scripts for cluster")
	currentScripts, err := client.GetClusterScripts(clusterID, clusterName)
	if err != nil {
		return fmt.Errorf("failed to get data retention scripts: %w", err)
	}

	actions := script.GetActions(definitions, currentScripts, script.ScriptConfig{
		ClusterName:     clusterName,
		ClusterId:       clusterID,
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
		err := client.UpdateDataRetentionScript(clusterID, s.ScriptId, s.Name, s.Description, s.FrequencyS, s.Script)
		if err != nil {
			errs = append(errs, err)
		}
	}

	for _, s := range actions.ToCreate {
		log.Infof("Creating script %s", s.Name)
		err := client.AddDataRetentionScript(clusterID, s.Name, s.Description, s.FrequencyS, s.Script)
		if err != nil {
			errs = append(errs, err)
		}
	}

	if len(errs) > 0 {
		return fmt.Errorf("errors while setting up data retention scripts: %v", errs)
	}

	log.Info("All done! The ClickHouse plugin is now configured.")
	return nil
}

func setupPixie(ctx context.Context, cfg config.Pixie, tries int, sleepTime time.Duration) (*pixie.Client, error) {
	apiKey := cfg.APIKey()
	host := cfg.Host()
	log.Infof("setupPixie: API Key length=%d, Host=%s", len(apiKey), host)

	for tries > 0 {
		// Use parent context - client stores this and uses it for all subsequent operations
		client, err := pixie.NewClient(ctx, apiKey, host)
		if err == nil {
			return client, nil
		}
		tries -= 1
		log.WithError(err).Warning("error creating Pixie API client")
		if tries > 0 {
			time.Sleep(sleepTime)
		}
	}
	return nil, fmt.Errorf("exceeded maximum number of retries")
}
