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
	"strings"

	"github.com/gofrs/uuid"
	"github.com/nats-io/nats.go"
	"github.com/nats-io/stan.go"
	"github.com/prometheus/client_golang/prometheus"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

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

var (
	natsErrorCount = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "nats_error_count",
		Help: "NATS message bus error",
	}, []string{"shardID", "vizierID", "messageKind", "errorKind"})
)

func init() {
	pflag.String("vzmgr_service", "kubernetes:///vzmgr-service.plc:51800", "The profile service url (load balancer/list is ok)")
	pflag.String("domain_name", "dev.withpixie.dev", "The domain name of Pixie Cloud")

	prometheus.MustRegister(natsErrorCount)
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

// Extracts information from the subject and return shard, vizierID, messageType.
func extractMessageInfo(subject string) (string, string, string) {
	vals := strings.Split(subject, ":")
	if len(vals) > 0 {
		strings.Split(vals[0], ".")
		if len(vals) >= 4 {
			return vals[1], vals[2], vals[3]
		}
	}
	return "", "", ""
}

func mustSetupNATSAndSTAN() (*nats.Conn, stan.Conn, msgbus.Streamer) {
	nc := msgbus.MustConnectNATS()
	stc := msgbus.MustConnectSTAN(nc, uuid.Must(uuid.NewV4()).String())
	strmr, err := msgbus.NewSTANStreamer(stc)
	if err != nil {
		log.WithError(err).Fatal("Could not start STAN streamer")
	}

	nc.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		if err != nil {
			log.WithError(err).
				WithField("Subject", subscription.Subject).
				Error("Got NATS error")
		}
		shard, vizierID, messageType := extractMessageInfo(subscription.Subject)
		switch err {
		case nats.ErrSlowConsumer:
			natsErrorCount.WithLabelValues(shard, vizierID, messageType, "ErrSlowConsumer").Inc()
		default:
			natsErrorCount.WithLabelValues(shard, vizierID, messageType, "ErrUnknown").Inc()
		}
	})
	return nc, stc, strmr
}

func main() {
	services.SetupService("vzconn-service", 51600)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

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
	nc, stc, strmr := mustSetupNATSAndSTAN()
	defer nc.Close()
	defer stc.Close()

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
