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
	"errors"
	"fmt"
	"net/http"
	"time"

	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	k8sErrors "k8s.io/apimachinery/pkg/api/errors"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/election"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/httpmiddleware"
	"px.dev/pixie/src/shared/services/server"
	"px.dev/pixie/src/shared/services/statusz"
	"px.dev/pixie/src/shared/status"
	controllers "px.dev/pixie/src/vizier/services/cloud_connector/bridge"
	"px.dev/pixie/src/vizier/services/cloud_connector/vizhealth"
	"px.dev/pixie/src/vizier/services/cloud_connector/vzmetrics"
)

func init() {
	pflag.String("cluster_id", "", "The Cluster ID to use for Pixie Cloud")
	pflag.String("nats_url", "pl-nats", "The URL of NATS")
	pflag.Duration("max_expected_clock_skew", 2000, "Duration in ms of expected maximum clock skew in a cluster")
	pflag.Duration("renew_period", 5000, "Duration in ms of the time to wait to renew lease")
	pflag.String("pod_namespace", "pl", "The namespace this pod runs in.")
	pflag.String("qb_service", "vizier-query-broker-svc", "The querybroker service url (load balancer/list is ok)")
	pflag.String("qb_port", "50300", "The querybroker service port")
	pflag.String("cluster_name", "", "The name of the user's K8s cluster")
	pflag.String("vizier_name", "", "The name of the user's K8s cluster, assigned by Pixie cloud")
	pflag.String("deploy_key", "", "The deploy key for the cluster")
	pflag.Bool("disable_auto_update", false, "Whether auto-update should be disabled")
	pflag.Duration("metrics_scrape_period", 15*time.Minute, "Period that the metrics scraper should run at.")
}
func newVzServiceClient() (vizierpb.VizierServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	qbAddr := fmt.Sprintf("%s.%s.svc:%s", viper.GetString("qb_service"), viper.GetString("pod_namespace"), viper.GetString("qb_port"))

	qbChannel, err := grpc.Dial(qbAddr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return vizierpb.NewVizierServiceClient(qbChannel), nil
}

// Checks to see if the cloud connector has successfully assigned a cluster ID.
type readinessCheck struct {
	bridge *controllers.Bridge
}

func (r *readinessCheck) Name() string {
	return "cluster-id"
}

func (r *readinessCheck) Check() error {
	s := r.bridge.GetStatus()
	if s == "" {
		return nil
	}
	return errors.New(string(s))
}

func main() {
	services.SetupService("cloud-connector", 50800)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	flush := services.InitSentryFromCRD(viper.GetString("cluster_id"),
		viper.GetString("pod_namespace"))
	defer flush()

	clusterID := viper.GetString("cluster_id")
	vizierID := uuid.FromStringOrNil(clusterID)

	assignedClusterName := viper.GetString("vizier_name")

	deployKey := viper.GetString("deploy_key")

	vzInfo, err := controllers.NewK8sVizierInfo(viper.GetString("cluster_name"), viper.GetString("pod_namespace"))
	if err != nil {
		log.WithError(err).Fatal("Could not get k8s info")
	}

	// Clean up cert-provisioner-job, if exists.
	certJob, err := vzInfo.GetJob("cert-provisioner-job")
	if err == nil && certJob != nil {
		err = vzInfo.DeleteJob("cert-provisioner-job")
		if err != nil && !k8sErrors.IsNotFound(err) {
			log.WithError(err).Info("Error deleting cert-provisioner-job")
		}
	}

	leaderMgr, err := election.NewK8sLeaderElectionMgr(
		viper.GetString("pod_namespace"),
		viper.GetDuration("max_expected_clock_skew"),
		viper.GetDuration("renew_period"),
		"cloud-conn-election",
	)

	if err != nil {
		log.WithError(err).Fatal("Failed to connect to leader election manager.")
	}
	// Cancel callback causes leader to resign.
	leaderCtx, cancel := context.WithCancel(context.Background())
	err = leaderMgr.Campaign(leaderCtx)
	if err != nil {
		log.WithError(err).Fatal("Failed to become leader")
	}

	resign := func() {
		log.Info("Resigning leadership")
		cancel()
	}
	// Resign leadership after the server stops.
	defer resign()

	qbVzClient, err := newVzServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init qb stub")
	}
	checker := vizhealth.NewChecker(viper.GetString("jwt_signing_key"), qbVzClient)
	defer checker.Stop()

	// Periodically clean up any completed jobs.
	quitCh := make(chan bool)
	go vzInfo.CleanupCronJob("etcd-defrag-job", 2*time.Hour, quitCh)
	defer close(quitCh)

	scraper := vzmetrics.NewScraper(viper.GetString("pod_namespace"), viper.GetDuration("metrics_scrape_period"))
	go scraper.Run()
	defer scraper.Stop()

	// We just use the current time in nanoseconds to mark the session ID. This will let the cloud side know that
	// the cloud connector restarted. Clock skew might make this incorrect, but we mostly want this for debugging.
	sessionID := time.Now().UnixNano()
	svr := controllers.New(vizierID, assignedClusterName, viper.GetString("jwt_signing_key"), deployKey, sessionID, nil, vzInfo, vzInfo, nil, checker, scraper.MetricsChannel())
	go svr.RunStream()
	defer svr.Stop()

	mux := http.NewServeMux()
	// Set up healthz endpoint.
	healthz.RegisterDefaultChecks(mux)
	// Set up readyz endpoint.
	healthz.InstallPathHandler(mux, "/readyz", &readinessCheck{svr})

	statusz.InstallPathHandler(mux, "/statusz", func() string {
		// Check state of the bridge.
		bridgeStatus := svr.GetStatus()
		if bridgeStatus != "" {
			return string(bridgeStatus)
		}

		// If bridge is functioning, check whether queries can be executed.
		_, err := checker.GetStatus()
		if err != nil {
			log.WithError(err).Info("health check returned unhealthy")
			return string(status.CloudConnectorBasicQueryFailed)
		}

		return ""
	})

	e := env.New("vizier")
	s := server.NewPLServer(e,
		httpmiddleware.WithBearerAuthMiddleware(e, mux))

	vizierpb.RegisterVizierDebugServiceServer(s.GRPCServer(), svr)

	s.Start()
	s.StopOnInterrupt()
}
