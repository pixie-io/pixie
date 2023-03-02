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
	"net/http"
	_ "net/http/pprof"

	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/cloud/shared/messages"
	"px.dev/pixie/src/cloud/vzconn/bridge"
	"px.dev/pixie/src/cloud/vzconn/vzconnpb"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/metrics"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/shared/services/server"
)

var natsErrorCounter *messages.NatsErrorCounter

func init() {
	pflag.String("vzmgr_service", "kubernetes:///vzmgr-service.plc:51800", "The profile service url (load balancer/list is ok)")
	pflag.String("domain_name", "dev.withpixie.dev", "The domain name of Pixie Cloud")

	natsErrorCounter = messages.NewNatsErrorCounter()
}

func newVZMgrClients() (vzmgrpb.VZMgrServiceClient, vzmgrpb.VZDeploymentServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, nil, err
	}

	vzmgrChannel, err := grpc.Dial(viper.GetString("vzmgr_service"), dialOpts...)
	if err != nil {
		return nil, nil, err
	}

	return vzmgrpb.NewVZMgrServiceClient(vzmgrChannel), vzmgrpb.NewVZDeploymentServiceClient(vzmgrChannel), nil
}

func mustSetupNATSAndJetStream() (*nats.Conn, msgbus.Streamer) {
	nc := msgbus.MustConnectNATS()
	js := msgbus.MustConnectJetStream(nc)
	strmr, err := msgbus.NewJetStreamStreamer(nc, js, msgbus.V2CDurableStream)
	if err != nil {
		log.WithError(err).Fatal("Could not start JetStream streamer")
	}

	nc.SetErrorHandler(natsErrorCounter.HandleNatsError)
	return nc, strmr
}

func main() {
	services.SetupService("vzconn-service", 51600)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	flush := services.InitDefaultSentry()
	defer flush()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)
	// VZConn is the backend for a GCLB and that health checks on "/" instead of the regular health check endpoint.
	healthz.InstallPathHandler(mux, "/")

	metrics.MustRegisterMetricsHandler(mux)

	// Communication from Vizier to VZConn is not auth'd via GRPC auth.
	serverOpts := &server.GRPCServerOptions{
		DisableAuth: map[string]bool{
			"/px.services.VZConnService/NATSBridge":               true,
			"/px.services.VZConnService/RegisterVizierDeployment": true,
		},
	}

	s := server.NewPLServerWithOptions(env.New(viper.GetString("domain_name")), mux, serverOpts)
	// Connect to NATS.
	nc, strmr := mustSetupNATSAndJetStream()
	defer nc.Close()

	vzmgrClient, vzdeployClient, err := newVZMgrClients()
	if err != nil {
		log.WithError(err).Fatal("failed to initialize vizer manager RPC client")
		panic(err)
	}
	svr := bridge.NewBridgeGRPCServer(vzmgrClient, vzdeployClient, nc, strmr)
	vzconnpb.RegisterVZConnServiceServer(s.GRPCServer(), svr)

	s.Start()
	s.StopOnInterrupt()
}
