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
	"net/http"
	_ "net/http/pprof"

	"cloud.google.com/go/storage"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/api/option"

	"px.dev/pixie/src/cloud/scriptmgr/controllers"
	"px.dev/pixie/src/cloud/scriptmgr/scriptmgrpb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/server"
)

func init() {
	pflag.String("bundle_bucket", "pixie-prod-artifacts", "GCS Bucket containing the bundle of scripts.")
	pflag.String("bundle_path", "script-bundles/bundle.json", "Path to bundle within bucket.")
}

func main() {
	services.SetupService("scriptmgr-service", 52000)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	s := server.NewPLServer(env.New(viper.GetString("domain_name")), mux)

	client, err := storage.NewClient(context.Background(), option.WithoutAuthentication())
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GCS client.")
	}

	svr := controllers.NewServer(
		viper.GetString("bundle_bucket"),
		viper.GetString("bundle_path"),
		stiface.AdaptClient(client))
	svr.Start()

	scriptmgrpb.RegisterScriptMgrServiceServer(s.GRPCServer(), svr)

	s.Start()
	s.StopOnInterrupt()
}
