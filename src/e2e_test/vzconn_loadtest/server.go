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
	"errors"
	"net/http"
	"time"

	"github.com/cenkalti/backoff/v4"
	"github.com/gofrs/uuid"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	batchv1 "k8s.io/api/batch/v1"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/httpmiddleware"
	"px.dev/pixie/src/shared/services/server"
	controllers "px.dev/pixie/src/vizier/services/cloud_connector/bridge"
)

const numConnections = 100

type fakeVZInfo struct {
	idx        int
	vzID       string
	clusterUID uuid.UUID
}

func (f *fakeVZInfo) GetVizierClusterInfo() (*cvmsgspb.VizierClusterInfo, error) {
	return &cvmsgspb.VizierClusterInfo{
		ClusterUID:  f.clusterUID.String(),
		ClusterName: f.clusterUID.String(),
	}, nil
}

func (f *fakeVZInfo) GetK8sState() *controllers.K8sState {
	return &controllers.K8sState{
		NumNodes:                      1,
		NumInstrumentedNodes:          1,
		ControlPlanePodStatuses:       nil,
		UnhealthyDataPlanePodStatuses: nil,
		K8sClusterVersion:             "v1.14.10-gke.27",
		LastUpdated:                   time.Now(),
	}
}

func (f *fakeVZInfo) LaunchJob(j *batchv1.Job) (*batchv1.Job, error) {
	return nil, nil
}

func (f *fakeVZInfo) ParseJobYAML(yamlStr string, imageTag map[string]string, envSubtitutions map[string]string) (*batchv1.Job, error) {
	return nil, nil
}

func (f *fakeVZInfo) CreateSecret(name string, literals map[string]string) error {
	return nil
}

func (f *fakeVZInfo) WaitForJobCompletion(name string) (bool, error) {
	return true, nil
}

func (f *fakeVZInfo) DeleteJob(name string) error {
	return nil
}

func (f *fakeVZInfo) GetJob(name string) (*batchv1.Job, error) {
	return nil, nil
}

func (f *fakeVZInfo) GetClusterUID() (string, error) {
	return f.clusterUID.String(), nil
}

func (f *fakeVZInfo) GetClusterID() (string, error) {
	return f.vzID, nil
}

func (f *fakeVZInfo) UpdateClusterID(vzID string) error {
	f.vzID = vzID
	return nil
}

func (f *fakeVZInfo) UpdateClusterName(string) error {
	return nil
}

func (f *fakeVZInfo) GetVizierPodLogs(string, bool, string) (string, error) {
	return "fake log", nil
}

func (f *fakeVZInfo) GetVizierPods() ([]*vizierpb.VizierPodStatus, []*vizierpb.VizierPodStatus, error) {
	fakeControlPlane := []*vizierpb.VizierPodStatus{
		{
			Name: "A pod",
		},
	}
	fakeAgents := []*vizierpb.VizierPodStatus{
		{
			Name: "Another pod",
		},
	}
	return fakeAgents, fakeControlPlane, nil
}

func (f *fakeVZInfo) UpdateClusterIDAnnotation(string) error {
	return nil
}

// fakeVZUpdater is an interface for faking calls to the Vizier CRD.
type fakeVZOperator struct{}

func (f *fakeVZOperator) UpdateCRDVizierVersion(string) (bool, error) {
	return false, nil
}

func (f *fakeVZOperator) GetVizierCRD() (*v1alpha1.Vizier, error) {
	return nil, errors.New("Could not get CRD")
}

// fakeVZHealthChecker always returns that the Vizier state is healthy.
type fakeVZHealthChecker struct{}

func (f *fakeVZHealthChecker) GetStatus() (time.Time, error) {
	return time.Now(), nil
}

func init() {
	pflag.String("cluster_id", "", "The Cluster ID to use for Pixie Cloud")
	pflag.String("nats_url", "pl-nats", "The URL of NATS")
	pflag.Duration("max_expected_clock_skew", 2000, "Duration in ms of expected maximum clock skew in a cluster")
	pflag.Duration("renew_period", 5000, "Duration in ms of the time to wait to renew lease")
	pflag.String("pod_namespace", "pl", "The namespace this pod runs in.")
	pflag.String("qb_service", "vizier-query-broker-svc", "The querybroker service url (load balancer/list is ok)")
	pflag.String("qb_port", "50300", "The querybroker service port")
	pflag.String("cluster_name", "", "The name of the user's K8s cluster")
	pflag.String("deploy_key", "", "The deploy key for the cluster")
	pflag.Bool("disable_auto_update", false, "Whether auto-update should be disabled")
}

func main() {
	services.SetupService("load-tester", 51600)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	flush := services.InitSentryFromCRD(viper.GetString("cluster_id"),
		viper.GetString("pod_namespace"))
	defer flush()

	deployKey := viper.GetString("deploy_key")

	mux := http.NewServeMux()
	// Set up healthz endpoint.
	healthz.RegisterDefaultChecks(mux)

	e := env.New("vizier")
	s := server.NewPLServer(e,
		httpmiddleware.WithBearerAuthMiddleware(e, mux))

	// We just use the current time in nanoseconds to mark the session ID. This will let the cloud side know that
	// the cloud connector restarted. Clock skew might make this incorrect, but we mostly want this for debugging.
	sessionID := time.Now().UnixNano()

	var err error
	var nc *nats.Conn

	connectNats := func() error {
		log.Info("Connecting to NATS...")
		nc, err = nats.Connect(viper.GetString("nats_url"),
			nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
			nats.RootCAs(viper.GetString("tls_ca_cert")))
		if err != nil {
			log.WithError(err).Error("Failed to connect to NATS")
		}
		return err
	}
	nc.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		log.WithField("Sub", subscription.Subject).
			WithError(err).
			Error("Error with NATS handler")
	})
	backOffOpts := backoff.NewExponentialBackOff()
	backOffOpts.InitialInterval = 30 * time.Second
	backOffOpts.Multiplier = 2
	backOffOpts.MaxElapsedTime = 30 * time.Minute

	err = backoff.Retry(connectNats, backOffOpts)
	if err != nil {
		log.WithError(err).Fatal("Could not connect to NATS")
	}
	log.Info("Successfully connected to NATS")

	// Track all svrs.
	cloudConnSvrs := make([]*controllers.Bridge, numConnections)
	go func() {
		i := 0
		for i < numConnections {
			clusterUID, _ := uuid.NewV4()
			svr := controllers.New(
				uuid.Nil,
				"",
				viper.GetString("jwt_signing_key"),
				deployKey,
				sessionID,
				nil,
				&fakeVZInfo{clusterUID: clusterUID, idx: i},
				&fakeVZOperator{},
				nc,
				&fakeVZHealthChecker{},
				nil)
			cloudConnSvrs[i] = svr
			go svr.RunStream()
			i++
		}
	}()

	defer func() {
		for _, svr := range cloudConnSvrs {
			if svr != nil {
				svr.Stop()
			}
		}
	}()

	s.Start()
	s.StopOnInterrupt()
}
