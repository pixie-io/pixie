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
	"context"
	"crypto/tls"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/cockroachdb/pebble"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"go.etcd.io/etcd/client/pkg/v3/transport"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/grpc"
	v1 "k8s.io/api/core/v1"

	version "px.dev/pixie/src/shared/goversion"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/election"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/httpmiddleware"
	"px.dev/pixie/src/shared/services/metrics"
	"px.dev/pixie/src/shared/services/server"
	"px.dev/pixie/src/vizier/services/metadata/controllers"
	"px.dev/pixie/src/vizier/services/metadata/controllers/agent"
	"px.dev/pixie/src/vizier/services/metadata/controllers/cronscript"
	"px.dev/pixie/src/vizier/services/metadata/controllers/k8smeta"
	"px.dev/pixie/src/vizier/services/metadata/controllers/tracepoint"
	"px.dev/pixie/src/vizier/services/metadata/metadataenv"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
	"px.dev/pixie/src/vizier/utils/datastore"
	"px.dev/pixie/src/vizier/utils/datastore/etcd"
	"px.dev/pixie/src/vizier/utils/datastore/pebbledb"
)

const (
	// pebbledbTTLDuration represents how often we evict from pebble.
	pebbledbTTLDuration = 1 * time.Minute
	// pebbleOpenDir is where the files live in the directory.
	pebbleOpenDir = "/metadata/pebble_20220209"
	// metadataBaseMount is the base volume mount if we are running a PVC backed metadata.
	metadataBaseMount = "/metadata"
)

func init() {
	pflag.String("md_etcd_server", "https://pl-etcd-client.pl.svc:2379", "The address to metadata etcd server.")
	pflag.String("cluster_id", "", "The Cluster ID to use for Pixie Cloud")
	pflag.Duration("max_expected_clock_skew", 2000, "Duration in ms of expected maximum clock skew in a cluster")
	pflag.Duration("renew_period", 5000, "Duration in ms of the time to wait to renew lease")
	pflag.String("pod_namespace", "pl", "The namespace this pod runs in. Used for leader elections")
	pflag.String("nats_url", "pl-nats", "The URL of NATS")
	pflag.Bool("use_etcd_operator", false, "Whether the etcd operator should be used instead of the persistent version.")

	// Metadata flags are set using the env vars in pl-cluster-config.
	// We historically set PL_ETCD_OPERATOR_ENABLED but not PL_USE_ETCD_OPERATOR in the configmap.
	// We also don't have a clean way to update  configmaps for existing deploys.
	// So instead just map PL_ETCD_OPERATOR_ENABLED to use_etcd_operator to make it work.
	// TODO: We should clean this up in the future and make these flags consistent.
	viper.BindEnv("use_etcd_operator", "PL_ETCD_OPERATOR_ENABLED")
}

func mustInitEtcdDatastore() (*etcd.DataStore, func()) {
	log.Infof("Using etcd: %s for metadata", viper.GetString("md_etcd_server"))
	var tlsConfig *tls.Config
	if !viper.GetBool("disable_ssl") {
		var err error
		tlsConfig, err = etcdTLSConfig()
		if err != nil {
			log.WithError(err).Fatal("Failed to load SSL for ETCD")
		}
	}

	// Connect to etcd.
	etcdClient, err := clientv3.New(clientv3.Config{
		Endpoints:   []string{viper.GetString("md_etcd_server")},
		DialTimeout: 5 * time.Second,
		TLS:         tlsConfig,
	})
	if err != nil {
		log.WithError(err).Fatalf("Failed to connect to etcd at %s. Please check status and logs for `pl-etcd` pods in the cluster.", viper.GetString("md_etcd_server"))
	}

	etcdMgr := controllers.NewEtcdManager(etcdClient)
	etcdMgr.Run()
	cleanupFunc := func() {
		etcdMgr.Stop()
		etcdClient.Close()
	}
	return etcd.New(etcdClient), cleanupFunc
}

func cleanupOldPebbleData() {
	files, err := os.ReadDir(metadataBaseMount)
	if err != nil {
		log.WithError(err).Fatal("Failed to read the metadata dir. Is the PVC correctly provisioned and running?")
	}
	pebblePath := strings.TrimPrefix(pebbleOpenDir, fmt.Sprintf("%s/", metadataBaseMount))

	for _, file := range files {
		if file.IsDir() && strings.HasPrefix(file.Name(), pebblePath) {
			// This is the current pebble dir, skip.
			continue
		}
		// Not the current pebble dir, likely an older dir, so just remove it.
		fullPath := filepath.Join(metadataBaseMount, file.Name())
		err = os.RemoveAll(fullPath)
		if err != nil {
			log.WithError(err).Infof("Failed to cleanup path %s", fullPath)
		}
	}
}

func mustInitPebbleDatastore() *pebbledb.DataStore {
	cleanupOldPebbleData()
	log.Infof("Using pebbledb: %s for metadata", pebbleOpenDir)
	pebbleDb, err := pebble.Open(pebbleOpenDir, &pebble.Options{})
	if err != nil {
		log.WithError(err).Fatal("Failed to open pebble database. If out of space, increase the storage size of the `metadata-pv-claim` PersistentVolumeClaim and restart the vizier-metadata pod")
	}
	return pebbledb.New(pebbleDb, pebbledbTTLDuration)
}

func etcdTLSConfig() (*tls.Config, error) {
	tlsCert := viper.GetString("client_tls_cert")
	tlsKey := viper.GetString("client_tls_key")
	tlsCACert := viper.GetString("tls_ca_cert")

	tlsInfo := transport.TLSInfo{
		CertFile:      tlsCert,
		KeyFile:       tlsKey,
		TrustedCAFile: tlsCACert,
	}

	return tlsInfo.ClientConfig()
}

func main() {
	services.SetupService("metadata", 50400)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	flush := services.InitSentryFromCRD(viper.GetString("cluster_id"),
		viper.GetString("pod_namespace"))
	defer flush()

	var nc *nats.Conn
	var err error
	if viper.GetBool("disable_ssl") {
		nc, err = nats.Connect(viper.GetString("nats_url"))
	} else {
		nc, err = nats.Connect(viper.GetString("nats_url"),
			nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
			nats.RootCAs(viper.GetString("tls_ca_cert")))
	}

	if err != nil {
		log.WithError(err).Fatal("Could not connect to NATS. Please check for the `pl-nats` pods in the namespace to confirm they are healthy and running.")
	}

	nc.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		log.WithError(err).
			WithField("sub", subscription.Subject).
			Error("Got nats error")
	})

	// Set up leader election.
	isLeader := false
	leaderMgr, err := election.NewK8sLeaderElectionMgr(
		viper.GetString("pod_namespace"),
		viper.GetDuration("max_expected_clock_skew"),
		viper.GetDuration("renew_period"),
		"metadata-election",
	)
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to leader election manager.")
	}

	// Cancel callback causes leader to resign.
	leaderCtx, cancel := context.WithCancel(context.Background())
	go func() {
		// Campaign in background. Metadata replicas that are not the leader should
		// do everything that the leader does, except write to the metadata store
		err = leaderMgr.Campaign(leaderCtx)
		if err != nil {
			log.WithError(err).Fatal("Failed to become leader")
		}
		log.Info("Gained leadership")
		isLeader = true
	}()

	// Resign leadership after the server stops.
	defer func() {
		log.Info("Resigning leadership")
		cancel()
	}()

	var dataStore datastore.MultiGetterSetterDeleterCloser
	var cleanupFunc func()
	if viper.GetBool("use_etcd_operator") {
		dataStore, cleanupFunc = mustInitEtcdDatastore()
		defer cleanupFunc()
	} else {
		dataStore = mustInitPebbleDatastore()
	}
	defer dataStore.Close()

	k8sMds := k8smeta.NewDatastore(dataStore)
	// Listen for K8s metadata updates.
	updateCh := make(chan *k8smeta.K8sResourceMessage)
	mdh := k8smeta.NewHandler(updateCh, k8sMds, k8sMds, nc)

	namespaces := []string{v1.NamespaceAll}
	if viper.IsSet("METADATA_NAMESPACES") {
		namespaces = strings.Split(viper.GetString("METADATA_NAMESPACES"), ",")
	}
	k8sMc, err := k8smeta.NewController(namespaces, updateCh)
	defer k8sMc.Stop()

	ads := agent.NewDatastore(dataStore, 24*time.Hour)
	agtMgr := agent.NewManager(ads, mdh, nc)

	schemaQuitCh := make(chan struct{})
	defer close(schemaQuitCh)
	go func() {
		schemaTimer := time.NewTicker(1 * time.Minute)
		defer schemaTimer.Stop()
		for {
			select {
			case <-schemaQuitCh:
				return
			case <-schemaTimer.C:
				schemaErr := ads.PruneComputedSchema()
				if schemaErr != nil {
					log.WithError(schemaErr).Info("Failed to prune computed schema")
				}
			}
		}
	}()

	tds := tracepoint.NewDatastore(dataStore)
	// Initialize tracepoint handler.
	tracepointMgr := tracepoint.NewManager(tds, agtMgr, 30*time.Second)
	defer tracepointMgr.Close()

	mc, err := controllers.NewMessageBusController(nc, agtMgr, tracepointMgr,
		mdh, &isLeader)

	if err != nil {
		log.WithError(err).Fatal("Failed to connect to message bus")
	}
	defer mc.Close()

	// Set up server.
	env, err := metadataenv.New("vizier")
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}
	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)
	metrics.MustRegisterMetricsHandlerNoDefaultMetrics(mux)

	svr := controllers.NewServer(env, dataStore, k8sMds, agtMgr, tracepointMgr)

	csDs := cronscript.NewDatastore(dataStore)
	cronScriptSvr := cronscript.New(csDs)

	log.Infof("Metadata Server: %s", version.GetVersion().ToString())

	// We bump up the max message size because agent metadata may be larger than 4MB. This is a
	// temporary change. In the future, we would like to page the agent metadata.
	maxMsgSize := grpc.MaxSendMsgSize(8 * 1024 * 1024)

	s := server.NewPLServer(env,
		httpmiddleware.WithBearerAuthMiddleware(env, mux), maxMsgSize)
	metadatapb.RegisterMetadataServiceServer(s.GRPCServer(), svr)
	metadatapb.RegisterMetadataTracepointServiceServer(s.GRPCServer(), svr)
	metadatapb.RegisterMetadataConfigServiceServer(s.GRPCServer(), svr)
	metadatapb.RegisterCronScriptStoreServiceServer(s.GRPCServer(), cronScriptSvr)

	s.Start()
	s.StopOnInterrupt()
}
