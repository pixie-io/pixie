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
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"io/ioutil"
	"net/http"
	_ "net/http/pprof"

	"github.com/gofrs/uuid"
	"github.com/nats-io/nats.go"
	"github.com/olivere/elastic/v7"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/cloud/indexer/controllers"
	"px.dev/pixie/src/cloud/indexer/md"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/shared/services/server"
)

func init() {
	pflag.String("es_url", "https://pl-elastic-es-http:9200", "The URL for the elastic cluster")
	pflag.String("es_ca_cert", "/es-certs/tls.crt", "The CA cert for elastic")
	pflag.String("es_user", "elastic", "The user for elastic")
	pflag.String("es_passwd", "elastic", "The password for elastic")
	pflag.String("vzmgr_service", "kubernetes:///vzmgr-service.plc:51800", "The profile service url (load balancer/list is ok)")
	pflag.String("domain_name", "dev.withpixie.dev", "The domain name of Pixie Cloud")
}

func newVZMgrClient() (vzmgrpb.VZMgrServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	vzmgrChannel, err := grpc.Dial(viper.GetString("vzmgr_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return vzmgrpb.NewVZMgrServiceClient(vzmgrChannel), nil
}

func getESHTTPSClient() (*http.Client, error) {
	caFile := viper.GetString("es_ca_cert")
	caCert, err := ioutil.ReadFile(caFile)
	if err != nil {
		return nil, err
	}
	caCertPool := x509.NewCertPool()
	ok := caCertPool.AppendCertsFromPEM(caCert)
	if !ok {
		return nil, fmt.Errorf("failed to append caCert to pool")
	}
	tlsConfig := &tls.Config{
		RootCAs: caCertPool,
	}
	tlsConfig.BuildNameToCertificate()
	transport := &http.Transport{
		TLSClientConfig: tlsConfig,
	}
	httpClient := &http.Client{
		Transport: transport,
	}
	return httpClient, nil
}

func mustConnectElastic() *elastic.Client {
	esURL := viper.GetString("es_url")
	httpClient, err := getESHTTPSClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to create HTTPS client")
	}
	es, err := elastic.NewClient(elastic.SetURL(esURL),
		elastic.SetHttpClient(httpClient),
		elastic.SetBasicAuth(viper.GetString("es_user"), viper.GetString("es_passwd")),
		// Sniffing seems to be broken with TLS, don't turn this on unless you want pain.
		elastic.SetSniff(false))
	if err != nil {
		log.WithError(err).Fatalf("Failed to connect to elastic at url: %s", esURL)
	}
	return es
}

func main() {
	services.SetupService("indexer-service", 51800)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	s := server.NewPLServer(env.New(viper.GetString("domain_name")), mux)
	nc := msgbus.MustConnectNATS()
	sc := msgbus.MustConnectSTAN(nc, uuid.Must(uuid.NewV4()).String())

	strmr, err := msgbus.NewSTANStreamer(sc)
	if err != nil {
		log.Fatal("Could not connect to streamer")
	}

	nc.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		log.WithError(err).
			WithField("sub", subscription.Subject).
			Error("Got nats error")
	})

	es := mustConnectElastic()
	err = md.InitializeMapping(es)
	if err != nil {
		log.WithError(err).Fatal("Could not initialize elastic mapping")
	}

	vzmgrClient, err := newVZMgrClient()
	if err != nil {
		log.WithError(err).Fatal("Could not connect to vzmgr")
	}

	indexer, err := controllers.NewIndexer(nc, vzmgrClient, strmr, es, "00", "ff")
	if err != nil {
		log.WithError(err).Fatal("Could not start indexer")
	}

	defer indexer.Stop()

	s.Start()
	s.StopOnInterrupt()
}
