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

	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/cloud/dnsmgr/controller"
	"px.dev/pixie/src/cloud/dnsmgr/dnsmgrenv"
	"px.dev/pixie/src/cloud/dnsmgr/dnsmgrpb"
	"px.dev/pixie/src/cloud/dnsmgr/schema"
	"px.dev/pixie/src/cloud/shared/pgmigrate"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/pg"
	"px.dev/pixie/src/shared/services/server"
)

func init() {
	pflag.String("dns_zone", "cluster-dev-withpixie-dev", "The zone to use for cloud DNS")
	pflag.String("dns_project", "pl-dev-infra", "The project to use for cloud DNS")
	pflag.String("domain_name", "withpixie.ai", "The domain name")
	pflag.Bool("use_default_dns_cert", false, "Whether to use the default DNS ssl cert")
}

func main() {
	services.SetupService("dnsmgr-service", 51900)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	db := pg.MustConnectDefaultPostgresDB()
	err := pgmigrate.PerformMigrationsUsingBindata(db, "dnsmgr_service_migrations",
		bindata.Resource(schema.AssetNames(), schema.Asset))
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}

	env := dnsmgrenv.New()
	dnsService, err := controller.NewCloudDNSService(
		viper.GetString("dns_zone"),
		viper.GetString("dns_project"),
		"/secrets/clouddns/dns_service_account.json",
	)

	if err != nil {
		log.WithError(err).Info("Failed to connect to Cloud DNS service. Unable to generate DNS records for Direct mode.")
		dnsService = nil
	}

	svr := controller.NewServer(env, dnsService, db)

	s := server.NewPLServer(env, mux)
	dnsmgrpb.RegisterDNSMgrServiceServer(s.GRPCServer(), svr)
	s.Start()
	s.StopOnInterrupt()
}
