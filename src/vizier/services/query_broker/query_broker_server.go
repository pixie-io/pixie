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
	"net"
	"net/http"
	"time"

	"github.com/cenkalti/backoff/v4"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/carnot/carnotpb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/httpmiddleware"
	"px.dev/pixie/src/shared/services/metrics"
	"px.dev/pixie/src/shared/services/server"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
	"px.dev/pixie/src/vizier/services/query_broker/controllers"
	"px.dev/pixie/src/vizier/services/query_broker/ptproxy"
	"px.dev/pixie/src/vizier/services/query_broker/querybrokerenv"
	scriptrunner "px.dev/pixie/src/vizier/services/query_broker/script_runner"
	"px.dev/pixie/src/vizier/services/query_broker/tracker"
)

const (
	querybrokerHostname = "vizier-query-broker-svc"
)

func init() {
	pflag.String("cluster_id", "", "The Cluster ID to use for Pixie Cloud")
	pflag.String("pod_ip_address", "", "The IP address of this pod to allow the agent to connect to this"+
		" particular query broker instance across multiple requests")
	pflag.String("mds_service", "vizier-metadata-svc", "The metadata service name")
	pflag.String("mds_port", "50400", "The querybroker service port")
	pflag.String("pod_namespace", "pl", "The namespace this pod runs in.")
	pflag.StringArray("cron_script_sources", scriptrunner.DefaultSources, "Where to find cron scripts (cloud, configmaps)")
}

// NewVizierServiceClient creates a new vz RPC client stub.
func NewVizierServiceClient(port uint) (vizierpb.VizierServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	// Note: This has to be localhost to pass the SSL cert verification.
	addr := fmt.Sprintf("localhost:%d", port)
	vzChannel, err := grpc.Dial(addr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return vizierpb.NewVizierServiceClient(vzChannel), nil
}

func main() {
	servicePort := uint(50300)
	services.SetupService("query-broker", servicePort)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	flush := services.InitSentryFromCRD(viper.GetString("cluster_id"),
		viper.GetString("pod_namespace"))
	defer flush()

	// For a given query, the agents may send multiple sets of results via the query broker.
	// We pass the pod IP address down to the agents in order to ensure that they continue to
	// communicate with the same query broker across a single request.
	podAddr := viper.GetString("pod_ip_address")
	if podAddr == "" {
		log.Fatal("Expected to receive pod IP address.")
	}
	qbIPAddr := net.JoinHostPort(podAddr, fmt.Sprintf("%d", servicePort))
	qbHostname := fmt.Sprintf("%s.%s.svc", querybrokerHostname, viper.GetString("pod_namespace"))
	env, err := querybrokerenv.New(qbIPAddr, qbHostname, "vizier")
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment.")
	}
	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)
	metrics.MustRegisterMetricsHandlerNoDefaultMetrics(mux)

	// Connect to metadata service.
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		log.WithError(err).Fatal("Could not get dial opts.")
	}
	dialOpts = append(dialOpts, grpc.WithBlock())
	dialOpts = append(dialOpts, grpc.WithDefaultCallOptions(grpc.MaxCallRecvMsgSize(8*1024*1024)))

	bOpts := backoff.NewExponentialBackOff()
	bOpts.InitialInterval = 15 * time.Second
	bOpts.MaxElapsedTime = 5 * time.Minute

	var mdsConn *grpc.ClientConn
	mdsAddr := fmt.Sprintf("%s.%s.svc:%s", viper.GetString("mds_service"), viper.GetString("pod_namespace"), viper.GetString("mds_port"))
	err = backoff.Retry(func() error {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()
		mdsConn, err = grpc.DialContext(ctx, mdsAddr, dialOpts...)
		if !errors.Is(err, context.DeadlineExceeded) {
			// Any errors that aren't timeouts are treated as permanent errors.
			return backoff.Permanent(err)
		}
		return err
	}, bOpts)

	if err != nil {
		log.WithError(err).Fatal("Failed to connect to Metadata Service.")
	}

	defer mdsConn.Close()

	mdsClient := metadatapb.NewMetadataServiceClient(mdsConn)
	mdtpClient := metadatapb.NewMetadataTracepointServiceClient(mdsConn)
	mdconfClient := metadatapb.NewMetadataConfigServiceClient(mdsConn)
	csClient := metadatapb.NewCronScriptStoreServiceClient(mdsConn)

	// Connect to NATS.
	var natsConn *nats.Conn
	if viper.GetBool("disable_ssl") {
		natsConn, err = nats.Connect("pl-nats")
	} else {
		natsConn, err = nats.Connect("pl-nats",
			nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
			nats.RootCAs(viper.GetString("tls_ca_cert")))
	}
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to NATS.")
	}
	natsConn.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		log.WithError(err).
			WithField("sub", subscription.Subject).
			Error("Got nats error")
	})

	dataPrivacy, err := controllers.CreateDataPrivacyManager(viper.GetString("pod_namespace"))
	if err != nil {
		log.WithError(err).Fatal("Failed to create data privacy manager.")
	}

	agentTracker := tracker.NewAgents(mdsClient, viper.GetString("jwt_signing_key"))
	agentTracker.Start()
	defer agentTracker.Stop()
	svr, err := controllers.NewServer(env, agentTracker, dataPrivacy, mdtpClient, mdconfClient, natsConn, controllers.NewQueryExecutorFromServer)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs.")
	}
	defer svr.Close()

	// For query broker we bump up the max message size since resuls might be larger than 4mb.
	maxMsgSize := grpc.MaxRecvMsgSize(8 * 1024 * 1024)

	s := server.NewPLServer(env,
		httpmiddleware.WithBearerAuthMiddleware(env, mux), maxMsgSize)

	carnotpb.RegisterResultSinkServiceServer(s.GRPCServer(), svr)
	vizierpb.RegisterVizierServiceServer(s.GRPCServer(), svr)

	// For the passthrough proxy we create a GRPC client to the current server. It appears really
	// hard to emulate the streaming GRPC connection and this helps keep the API straightforward.
	vzServiceClient, err := NewVizierServiceClient(servicePort)
	if err != nil {
		log.WithError(err).Fatal("Failed to init vzservice client.")
	}

	// Start passthrough proxy.
	ptProxy, err := ptproxy.NewPassThroughProxy(natsConn, vzServiceClient)
	if err != nil {
		log.WithError(err).Fatal("Failed to start passthrough proxy.")
	}
	go func() {
		err := ptProxy.Run()
		if err != nil {
			log.WithError(err).Error("Passthrough proxy failed to run")
		}
	}()
	defer ptProxy.Close()

	// Start cron script runner.
	sources := scriptrunner.Sources(
		natsConn,
		csClient,
		viper.GetString("jwt_signing_key"),
		viper.GetString("pod_namespace"),
		viper.GetStringSlice("cron_script_sources"),
	)
	sr := scriptrunner.New(csClient, vzServiceClient, viper.GetString("jwt_signing_key"), sources...)

	// Load the scripts and start the background sync.
	go func() {
		err := sr.SyncScripts()
		if err != nil {
			log.WithError(err).Error("Failed to sync cron scripts")
		}
	}()

	s.Start()
	s.StopOnInterrupt()
}
