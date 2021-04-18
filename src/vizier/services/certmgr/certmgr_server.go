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
	"time"

	"github.com/gofrs/uuid"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/server"
	"px.dev/pixie/src/vizier/services/certmgr/certmgrenv"
	certmgrpb "px.dev/pixie/src/vizier/services/certmgr/certmgrpb"
	"px.dev/pixie/src/vizier/services/certmgr/controller"
)

func init() {
	pflag.String("namespace", "pl", "The namespace of Vizier")
	pflag.String("cluster_id", "", "The Cluster ID to use for Pixie Cloud")
	pflag.String("nats_url", "pl-nats", "The URL of NATS")
}

func main() {
	services.SetupService("certmgr-service", 50900)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	flush := services.InitDefaultSentry(viper.GetString("cluster_id"))
	defer flush()

	natsWait := make(chan struct{})
	var nc *nats.Conn
	var err error

	go func() {
		nc, err = nats.Connect(viper.GetString("nats_url"),
			nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
			nats.RootCAs(viper.GetString("tls_ca_cert")))
		if err != nil {
			log.WithError(err).Fatal("Failed to connect to NATS.")
		}
		close(natsWait)
	}()

	select {
	case <-natsWait:
		log.Info("Connected to NATS")
	case <-time.After(1 * time.Minute):
		log.WithError(err).Fatal("Timed out: failed to connect to NATS.")
	}

	clusterID, err := uuid.FromString(viper.GetString("cluster_id"))
	if err != nil {
		log.WithError(err).Fatal("Failed to parse passed in cluster ID")
	}

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	k8sWait := make(chan struct{})
	var k8sAPI *controller.K8sAPIImpl

	go func() {
		k8sAPI, err = controller.NewK8sAPI(viper.GetString("namespace"))
		if err != nil {
			log.WithError(err).Fatal("Failed to connect to K8S API")
		}
		close(k8sWait)
	}()

	select {
	case <-k8sWait:
		log.Info("Connected to K8s API")
	case <-time.After(1 * time.Minute):
		log.WithError(err).Fatal("Timed out: failed to connect to K8s API.")
	}

	env := certmgrenv.New("vizier")
	svr := controller.NewServer(env, clusterID, nc, k8sAPI)
	go svr.CertRequester()
	defer svr.StopCertRequester()

	s := server.NewPLServer(env, mux)
	certmgrpb.RegisterCertMgrServiceServer(s.GRPCServer(), svr)
	s.Start()
	s.StopOnInterrupt()
}
