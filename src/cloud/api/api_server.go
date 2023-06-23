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
	"net/http"
	_ "net/http/pprof"
	"strings"
	"time"

	"github.com/gorilla/handlers"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/cloud/api/apienv"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/ptproxy"
	"px.dev/pixie/src/cloud/autocomplete"
	"px.dev/pixie/src/cloud/shared/esutils"
	"px.dev/pixie/src/cloud/shared/idprovider"
	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/shared/services"
	svcEnv "px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/handler"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/shared/services/server"
	"px.dev/pixie/src/utils/script"
)

const defaultBundleFile = "https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-core.json"
const ossBundleFile = "https://artifacts.px.dev/pxl_scripts/bundle.json"

func init() {
	pflag.String("domain_name", "dev.withpixie.dev", "The domain name of Pixie Cloud")
	pflag.String("elastic_service", "https://pl-elastic-es-http.plc-dev.svc.cluster.local:9200", "The url of the elasticsearch cluster")
	pflag.String("elastic_ca_cert", "/elastic-certs/ca.crt", "CA Cert for elastic cluster")
	pflag.String("elastic_tls_cert", "/elastic-certs/tls.crt", "TLS Cert for elastic cluster")
	pflag.String("elastic_tls_key", "/elastic-certs/tls.key", "TLS Key for elastic cluster")
	pflag.String("elastic_username", "elastic", "Username for access to elastic cluster")
	pflag.String("elastic_password", "", "Password for access to elastic")
	pflag.String("md_index_name", "", "The elastic index name for metadata.")
	pflag.String("allowed_origins", "", "The allowed origins for CORS")

	pflag.String("auth_connector_name", "", "If any, the name of the auth connector to be used with Pixie")
	pflag.String("auth_connector_callback_url", "", "If any, the callback URL for the auth connector")
}

func main() {
	services.SetupService("api-service", 51200)
	services.SetupSSLClientFlags()
	vzshard.SetupFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	flush := services.InitDefaultSentry()
	defer flush()

	ac, err := controllers.NewAuthClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init auth client")
	}

	pc, err := apienv.NewProfileServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init profile client")
	}

	oc, err := apienv.NewOrgServiceClient()
	if err != nil {
		log.WithError(err).Error("Failed to init org client")
	}

	cm, err := apienv.NewConfigManagerServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init config manager client")
	}

	vc, vk, err := apienv.NewVZMgrServiceClients()
	if err != nil {
		log.WithError(err).Fatal("Failed to init vzmgr clients")
	}

	at, err := apienv.NewArtifactTrackerClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init artifact tracker client")
	}

	ak, err := controllers.NewAPIKeyClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init API key client")
	}

	oa, err := idprovider.NewHydraKratosClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init Hydra + Kratos idprovider client")
	}

	ps, drps, err := apienv.NewPluginServiceClients()
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to plugin service")
	}

	env, err := apienv.New(ac, pc, oc, vk, ak, vc, at, oa, cm, ps, drps)
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}

	// Connect to NATS.
	nc := msgbus.MustConnectNATS()

	esConfig := &esutils.Config{
		URL:        []string{viper.GetString("elastic_service")},
		User:       viper.GetString("elastic_username"),
		Passwd:     viper.GetString("elastic_password"),
		CaCertFile: viper.GetString("elastic_ca_cert"),
	}
	es, err := esutils.NewEsClient(esConfig)
	if err != nil {
		log.WithError(err).Fatal("Could not connect to elastic")
	}

	mux := http.NewServeMux()
	mux.Handle("/api/auth/signup", handler.New(env, controllers.AuthSignupHandler))
	mux.Handle("/api/auth/login", handler.New(env, controllers.AuthLoginHandler))
	mux.Handle("/api/auth/loginEmbed", handler.New(env, controllers.AuthLoginHandlerEmbed))
	mux.Handle("/api/auth/logout", handler.New(env, controllers.AuthLogoutHandler))
	mux.Handle("/api/auth/refetch", handler.New(env, controllers.AuthRefetchHandler))
	mux.Handle("/api/auth/oauth/login", handler.New(env, controllers.AuthOAuthLoginHandler))
	// This is an unauthenticated path that will check and validate if a particular domain
	// is available for registration. This need to be unauthenticated because we need to check this before
	// the user registers.
	mux.Handle("/api/authorized", controllers.WithAugmentedAuthMiddleware(env, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "OK")
	})))

	if viper.GetString("auth_connector_name") != "" {
		mux.Handle(fmt.Sprintf("/api/auth/%s", viper.GetString("auth_connector_name")), handler.New(env, controllers.AuthConnectorHandler))
	}

	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	// API service needs to convert any cookies into an augmented token in bearer auth.
	serverOpts := &server.GRPCServerOptions{
		AuthMiddleware: func(ctx context.Context, e svcEnv.Env) (string, error) {
			apiEnv, ok := e.(apienv.APIEnv)
			if !ok {
				return "", errors.New("Could not convert env to apiEnv")
			}
			return controllers.GetAugmentedTokenGRPC(ctx, apiEnv)
		},
		DisableAuth: map[string]bool{
			"/px.cloudapi.ArtifactTracker/GetArtifactList":    true,
			"/px.cloudapi.ArtifactTracker/GetDownloadLink":    true,
			"/pl.cloudapi.ArtifactTracker/GetArtifactList":    true,
			"/pl.cloudapi.ArtifactTracker/GetDownloadLink":    true,
			"/px.cloudapi.ConfigService/GetConfigForVizier":   true,
			"/px.cloudapi.ConfigService/GetConfigForOperator": true,
			"/px.cloudapi.AuthService/Login":                  true,
		},
	}

	domainName := viper.GetString("domain_name")
	allowedOrigins := []string{"https://" + domainName, "https://work." + domainName}
	if viper.GetString("allowed_origins") != "" {
		allowedOrigins = append(allowedOrigins, strings.Split(viper.GetString("allowed_origins"), ",")...)
	}
	s := server.NewPLServerWithOptions(env, handlers.CORS(services.DefaultCORSConfig(allowedOrigins)...)(mux), serverOpts)

	imageAuthServer := &controllers.VizierImageAuthServer{}
	cloudpb.RegisterVizierImageAuthorizationServer(s.GRPCServer(), imageAuthServer)

	artifactTrackerServer := controllers.ArtifactTrackerServer{
		ArtifactTrackerClient: at,
	}
	cloudpb.RegisterArtifactTrackerServer(s.GRPCServer(), artifactTrackerServer)

	cis := &controllers.VizierClusterInfo{VzMgr: vc, ArtifactTrackerClient: at}
	cloudpb.RegisterVizierClusterInfoServer(s.GRPCServer(), cis)

	vdks := &controllers.VizierDeploymentKeyServer{VzDeploymentKey: vk}
	cloudpb.RegisterVizierDeploymentKeyManagerServer(s.GRPCServer(), vdks)

	aks := &controllers.APIKeyServer{APIKeyClient: ak}
	cloudpb.RegisterAPIKeyManagerServer(s.GRPCServer(), aks)

	authServer := &controllers.AuthServer{AuthClient: ac}
	cloudpb.RegisterAuthServiceServer(s.GRPCServer(), authServer)

	vpt := ptproxy.NewVizierPassThroughProxy(nc, vc)
	vizierpb.RegisterVizierServiceServer(s.GRPCServer(), vpt)
	vizierpb.RegisterVizierDebugServiceServer(s.GRPCServer(), vpt)

	sm, err := apienv.NewScriptMgrServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init scriptmgr client.")
	}
	sms := &controllers.ScriptMgrServer{ScriptMgr: sm}
	cloudpb.RegisterScriptMgrServer(s.GRPCServer(), sms)

	mdIndexName := viper.GetString("md_index_name")
	if mdIndexName == "" {
		log.Fatal("Must specify a name for the elastic index.")
	}
	esSuggester, err := autocomplete.NewElasticSuggester(es, mdIndexName, "scripts", pc)
	if err != nil {
		log.WithError(err).Fatal("Failed to start elastic suggester")
	}

	var br *script.BundleManager
	var bundleErr error
	updateBundle := func() {
		// Requiring the bundle manager in the API service is temporary until we
		// start indexing scripts.
		br, bundleErr = script.NewBundleManagerWithOrg([]string{defaultBundleFile, ossBundleFile}, "", "")
		if bundleErr != nil {
			log.WithError(bundleErr).Error("Failed to init bundle manager")
			br = nil
		}
		esSuggester.UpdateScriptBundle(br)
	}

	quitCh := make(chan bool)
	go func() {
		updateBundle()
		scriptTimer := time.NewTicker(30 * time.Second)
		defer scriptTimer.Stop()
		for {
			select {
			case <-quitCh:
				return
			case <-scriptTimer.C:
				updateBundle()
			}
		}
	}()
	defer close(quitCh)

	as := &controllers.AutocompleteServer{Suggester: esSuggester}
	cloudpb.RegisterAutocompleteServiceServer(s.GRPCServer(), as)

	os := &controllers.OrganizationServiceServer{ProfileServiceClient: pc, AuthServiceClient: ac, OrgServiceClient: oc}
	cloudpb.RegisterOrganizationServiceServer(s.GRPCServer(), os)

	us := &controllers.UserServiceServer{ProfileServiceClient: pc, OrgServiceClient: oc}
	cloudpb.RegisterUserServiceServer(s.GRPCServer(), us)

	cs := &controllers.ConfigServiceServer{ConfigServiceClient: cm}
	cloudpb.RegisterConfigServiceServer(s.GRPCServer(), cs)

	pss := &controllers.PluginServiceServer{PluginServiceClient: ps, DataRetentionPluginServiceClient: drps}
	cloudpb.RegisterPluginServiceServer(s.GRPCServer(), pss)

	gqlEnv := controllers.GraphQLEnv{
		ArtifactTrackerServer: artifactTrackerServer,
		VizierClusterInfo:     cis,
		VizierDeployKeyMgr:    vdks,
		APIKeyMgr:             aks,
		ScriptMgrServer:       sms,
		AutocompleteServer:    as,
		OrgServer:             os,
		UserServer:            us,
		PluginServer:          pss,
	}

	mux.Handle("/api/graphql", controllers.WithAugmentedAuthMiddleware(env, controllers.NewGraphQLHandler(gqlEnv)))

	mux.Handle("/api/unauthenticated/graphql", controllers.NewUnauthenticatedGraphQLHandler(gqlEnv))

	s.Start()
	s.StopOnInterrupt()
}
