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
	"github.com/olivere/elastic/v7"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/cloud/indexer/controllers"
	"px.dev/pixie/src/cloud/indexer/md"
	"px.dev/pixie/src/cloud/shared/esutils"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/metrics"
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

	pflag.String("md_index_name", "", "The elastic index name for metadata.")
	pflag.String("md_index_max_age", "", "The amount of time before rolling over the elastic index as a string, eg '30d'")
	pflag.String("md_index_delete_after", "", "The amount of time after rollover to delete old elastic indices, as a string, eg '30d'")
	pflag.Int("md_index_replicas", 4, "The number of replicas to setup for the metadata index.")
	pflag.Bool("md_manual_index_management", false, "Skip creation of managed elastic indices. Requires manually deploying an elastic index with md_index_name")
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

func mustConnectElastic() *elastic.Client {
	esURL := viper.GetString("es_url")

	es, err := esutils.NewEsClient(&esutils.Config{
		URL:        []string{esURL},
		User:       viper.GetString("es_user"),
		Passwd:     viper.GetString("es_passwd"),
		CaCertFile: viper.GetString("es_ca_cert"),
	})

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

	flush := services.InitDefaultSentry()
	defer flush()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)
	metrics.MustRegisterMetricsHandler(mux)

	s := server.NewPLServer(env.New(viper.GetString("domain_name")), mux)
	nc := msgbus.MustConnectNATS()
	js := msgbus.MustConnectJetStream(nc)

	strmr, err := msgbus.NewJetStreamStreamer(nc, js, msgbus.MetadataIndexStream)
	if err != nil {
		log.Fatal("Could not connect to streamer")
	}

	nc.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		log.WithError(err).
			WithField("sub", subscription.Subject).
			Error("Got nats error")
	})

	es := mustConnectElastic()

	indexName := viper.GetString("md_index_name")
	if indexName == "" {
		log.Fatal("Must specify a name for the elastic index.")
	}
	replicas := viper.GetInt("md_index_replicas")

	maxAge := viper.GetString("md_index_max_age")
	if maxAge == "" {
		log.Fatal("Must specify a max age for the elastic index.")
	}
	deleteAfter := viper.GetString("md_index_delete_after")
	if deleteAfter == "" {
		log.Fatal("Must specify a delete after time for the rolled over elastic indices.")
	}

	err = md.InitializeMapping(es, indexName, replicas, maxAge, deleteAfter, viper.GetBool("md_manual_index_management"))
	if err != nil {
		log.WithError(err).Fatal("Could not initialize elastic mapping")
	}

	vzmgrClient, err := newVZMgrClient()
	if err != nil {
		log.WithError(err).Fatal("Could not connect to vzmgr")
	}

	indexer, err := controllers.NewIndexer(nc, vzmgrClient, strmr, es, indexName, "00", "ff")
	if err != nil {
		log.WithError(err).Fatal("Could not start indexer")
	}

	defer indexer.Stop()

	s.Start()
	s.StopOnInterrupt()
}
