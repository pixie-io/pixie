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

package config

import (
	"context"
	"fmt"
	"os"
	"strconv"
	"strings"
	"sync"

	log "github.com/sirupsen/logrus"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"

	"px.dev/pixie/src/utils/shared/k8s"
)

const (
	envVerbose                  = "VERBOSE"
	envClickHouseDSN            = "CLICKHOUSE_DSN"
	envPixieClusterID           = "PIXIE_CLUSTER_ID"
	envPixieEndpoint            = "PIXIE_ENDPOINT"
	envPixieAPIKey              = "PIXIE_API_KEY"
	envClusterName              = "CLUSTER_NAME"
	envCollectInterval          = "COLLECT_INTERVAL_SEC"
	envDetectionInterval        = "DETECTION_INTERVAL_SEC"
	envDetectionLookback        = "DETECTION_LOOKBACK_SEC"
	defPixieHostname            = "work.withpixie.ai:443"
	boolTrue                    = "true"
	defCollectInterval          = 30
	defDetectionInterval        = 10
	defDetectionLookback        = 15
)

var (
	integrationVersion = "0.0.0"
	gitCommit          = ""
	buildDate          = ""
	once               sync.Once
	instance           Config
)

// findVizierNamespace looks for the namespace that the vizier is running in.
func findVizierNamespace(clientset *kubernetes.Clientset) (string, error) {
	vzPods, err := clientset.CoreV1().Pods("").List(context.Background(), metav1.ListOptions{
		LabelSelector: "component=vizier",
	})
	if err != nil {
		return "", err
	}

	if len(vzPods.Items) == 0 {
		return "", fmt.Errorf("no vizier pods found")
	}

	return vzPods.Items[0].Namespace, nil
}

// getK8sConfig attempts to read configuration from Kubernetes secrets and configmaps.
// Returns (clusterID, apiKey, clusterName, host, error).
func getK8sConfig() (string, string, string, string, error) {
	// Try in-cluster config first (when running in K8s)
	config, err := rest.InClusterConfig()
	if err != nil {
		log.WithError(err).Debug("In-cluster config not available, trying kubeconfig...")
		// Fall back to kubeconfig for local/adhoc testing
		config = k8s.GetConfig()
		if config == nil {
			return "", "", "", "", fmt.Errorf("unable to get kubernetes config")
		}
	} else {
		log.Debug("Using in-cluster Kubernetes config")
	}

	clientset, err := kubernetes.NewForConfig(config)
	if err != nil {
		return "", "", "", "", fmt.Errorf("unable to create kubernetes clientset: %w", err)
	}

	vzNs, err := findVizierNamespace(clientset)
	if err != nil || vzNs == "" {
		return "", "", "", "", fmt.Errorf("unable to find vizier namespace: %w", err)
	}

	// Get cluster-id and cluster-name from pl-cluster-secrets
	clusterSecrets := k8s.GetSecret(clientset, vzNs, "pl-cluster-secrets")
	if clusterSecrets == nil {
		return "", "", "", "", fmt.Errorf("unable to get pl-cluster-secrets")
	}

	clusterID := ""
	if cID, ok := clusterSecrets.Data["cluster-id"]; ok {
		clusterID = string(cID)
	}

	clusterName := ""
	if cn, ok := clusterSecrets.Data["cluster-name"]; ok {
		clusterName = string(cn)
	}

	// Note: pl-deploy-secrets contains the deployment key (for registering vizier),
	// not the user API key (for accessing cloud APIs). The user API key must be
	// provided via PIXIE_API_KEY environment variable.
	apiKey := ""

	// Get PL_CLOUD_ADDR from pl-cloud-config
	cloudConfig, err := clientset.CoreV1().ConfigMaps(vzNs).Get(context.Background(), "pl-cloud-config", metav1.GetOptions{})
	host := ""
	if err == nil {
		if addr, ok := cloudConfig.Data["PL_CLOUD_ADDR"]; ok {
			host = addr
		}
	}

	return clusterID, apiKey, clusterName, host, nil
}

func GetConfig() (Config, error) {
	var err error
	once.Do(func() {
		err = setUpConfig()
	})
	return instance, err
}

func setUpConfig() error {
	log.SetLevel(log.InfoLevel)

	// Try to read configuration from environment variables first
	clickhouseDSN := os.Getenv(envClickHouseDSN)
	pixieClusterID := os.Getenv(envPixieClusterID)
	pixieAPIKey := os.Getenv(envPixieAPIKey)
	clusterName := os.Getenv(envClusterName)
	pixieHost := getEnvWithDefault(envPixieEndpoint, defPixieHostname)
	enableDebug := os.Getenv(envVerbose)

	if strings.EqualFold(enableDebug, boolTrue) {
		log.SetLevel(log.DebugLevel)
	}

	log.Debugf("Config from environment - ClickHouse DSN: %s", clickhouseDSN)
	log.Debugf("Config from environment - Pixie Cluster ID: %s", pixieClusterID)
	log.Debugf("Config from environment - Pixie API Key: %s", pixieAPIKey)
	log.Debugf("Config from environment - Cluster Name: %s", clusterName)
	log.Debugf("Config from environment - Pixie Host: %s", pixieHost)

	// If key values are not set via environment, try reading from Kubernetes
	// Note: API key cannot be read from K8s (only deployment key is there), must be provided via env
	if pixieClusterID == "" || clusterName == "" || pixieHost == defPixieHostname {
		log.Info("Attempting to read Pixie configuration from Kubernetes resources...")
		k8sClusterID, _, k8sClusterName, k8sHost, err := getK8sConfig()
		if err != nil {
			log.WithError(err).Warn("Failed to read configuration from Kubernetes, will use environment variables only")
		} else {
			// Use k8s values only if env vars are not set
			if pixieClusterID == "" {
				pixieClusterID = k8sClusterID
				log.Debugf("Using cluster ID from Kubernetes: %s", pixieClusterID)
			}
			if clusterName == "" {
				clusterName = k8sClusterName
				log.Debugf("Using cluster name from Kubernetes: %s", clusterName)
			}
			if pixieHost == defPixieHostname && k8sHost != "" {
				pixieHost = k8sHost
				log.Debugf("Using host from Kubernetes: %s", pixieHost)
			}
		}
	}

	log.Debugf("Final config - Pixie Cluster ID: %s", pixieClusterID)
	log.Debugf("Final config - Pixie API Key: %s", pixieAPIKey)
	log.Debugf("Final config - Cluster Name: %s", clusterName)
	log.Debugf("Final config - Pixie Host: %s", pixieHost)
	log.Debugf("Final config - ClickHouse DSN: %s", clickhouseDSN)

	collectInterval, err := getIntEnvWithDefault(envCollectInterval, defCollectInterval)
	if err != nil {
		return err
	}

	detectionInterval, err := getIntEnvWithDefault(envDetectionInterval, defDetectionInterval)
	if err != nil {
		return err
	}

	detectionLookback, err := getIntEnvWithDefault(envDetectionLookback, defDetectionLookback)
	if err != nil {
		return err
	}

	instance = &config{
		settings: &settings{
			buildDate: buildDate,
			commit:    gitCommit,
			version:   integrationVersion,
		},
		worker: &worker{
			clusterName:        clusterName,
			pixieClusterID:     pixieClusterID,
			collectInterval:    collectInterval,
			detectionInterval:  detectionInterval,
			detectionLookback:  detectionLookback,
		},
		clickhouse: &clickhouse{
			dsn:       clickhouseDSN,
			userAgent: "pixie-clickhouse/" + integrationVersion,
		},
		pixie: &pixie{
			apiKey:    pixieAPIKey,
			clusterID: pixieClusterID,
			host:      pixieHost,
		},
	}
	return instance.validate()
}

func getEnvWithDefault(key, defaultValue string) string {
	value := os.Getenv(key)
	if value == "" {
		return defaultValue
	}
	return value
}

func getIntEnvWithDefault(key string, defaultValue int64) (int64, error) {
	value := os.Getenv(key)
	if value == "" {
		return defaultValue, nil
	}
	i, err := strconv.ParseInt(value, 10, 64)
	if err != nil {
		return 0, fmt.Errorf("Environment variable %s is not an integer.", key)
	}
	return i, nil
}

type Config interface {
	Verbose() bool
	Settings() Settings
	ClickHouse() ClickHouse
	Pixie() Pixie
	Worker() Worker
	validate() error
}

type config struct {
	verbose    bool
	worker     Worker
	clickhouse ClickHouse
	pixie      Pixie
	settings   Settings
}

func (c *config) validate() error {
	if err := c.Pixie().validate(); err != nil {
		return fmt.Errorf("error validating pixie config: %w", err)
	}
	if err := c.Worker().validate(); err != nil {
		return fmt.Errorf("error validating worker config: %w", err)
	}
	return c.ClickHouse().validate()
}

func (c *config) Settings() Settings {
	return c.settings
}

func (c *config) Verbose() bool {
	return c.verbose
}

func (c *config) ClickHouse() ClickHouse {
	return c.clickhouse
}

func (c *config) Worker() Worker {
	return c.worker
}

func (c *config) Pixie() Pixie {
	return c.pixie
}

type Settings interface {
	Version() string
	Commit() string
	BuildDate() string
}

type settings struct {
	buildDate string
	commit    string
	version   string
}

func (s *settings) Version() string {
	return s.version
}

func (s *settings) Commit() string {
	return s.commit
}

func (s *settings) BuildDate() string {
	return s.buildDate
}

type ClickHouse interface {
	DSN() string
	UserAgent() string
	validate() error
}

type clickhouse struct {
	dsn       string
	userAgent string
}

func (c *clickhouse) validate() error {
	if c.dsn == "" {
		return fmt.Errorf("missing required env variable '%s'", envClickHouseDSN)
	}
	return nil
}

func (c *clickhouse) DSN() string {
	return c.dsn
}

func (c *clickhouse) UserAgent() string {
	return c.userAgent
}

type Pixie interface {
	APIKey() string
	ClusterID() string
	Host() string
	validate() error
}

type pixie struct {
	apiKey    string
	clusterID string
	host      string
}

func (p *pixie) validate() error {
	if p.apiKey == "" {
		return fmt.Errorf("missing required env variable '%s'", envPixieAPIKey)
	}
	if p.clusterID == "" {
		return fmt.Errorf("missing required env variable '%s'", envPixieClusterID)
	}
	return nil
}

func (p *pixie) APIKey() string {
	return p.apiKey
}

func (p *pixie) ClusterID() string {
	return p.clusterID
}

func (p *pixie) Host() string {
	return p.host
}

type Worker interface {
	ClusterName() string
	PixieClusterID() string
	CollectInterval() int64
	DetectionInterval() int64
	DetectionLookback() int64
	validate() error
}

type worker struct {
	clusterName        string
	pixieClusterID     string
	collectInterval    int64
	detectionInterval  int64
	detectionLookback  int64
}

func (a *worker) validate() error {
	if a.clusterName == "" {
		return fmt.Errorf("missing required env variable '%s'", envClusterName)
	}
	return nil
}

func (a *worker) ClusterName() string {
	return a.clusterName
}

func (a *worker) PixieClusterID() string {
	return a.pixieClusterID
}

func (a *worker) CollectInterval() int64 {
	return a.collectInterval
}

func (a *worker) DetectionInterval() int64 {
	return a.detectionInterval
}

func (a *worker) DetectionLookback() int64 {
	return a.detectionLookback
}
