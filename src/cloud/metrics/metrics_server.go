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

	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/cloud/metrics/controller"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/shared/services/server"
)

func init() {
	pflag.String("artifact_tracker_service", "kubernetes:///artifact-tracker-service.plc:50750", "The artifact tracker service url (load balancer/list is ok)")
	pflag.String("prod_sentry", "", "Key for prod Viziers that is used to send errors and stacktraces to Sentry.")
	pflag.String("dev_sentry", "", "Key for dev Viziers that is used to send errors and stacktraces to Sentry.")
}

func main() {
	services.SetupService("metrics-service", 50800)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	// Connect to NATS.
	nc := msgbus.MustConnectNATS()
	_ = controller.NewServer(nc)

	s := server.NewPLServer(env.New(viper.GetString("domain_name")), mux)
	s.Start()
	s.StopOnInterrupt()
}
