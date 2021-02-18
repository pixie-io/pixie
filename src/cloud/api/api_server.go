package main

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"time"

	"github.com/nats-io/nats.go"
	"github.com/spf13/viper"

	"github.com/gorilla/handlers"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	public_cloudapipb "pixielabs.ai/pixielabs/src/api/public/cloudapipb"
	public_vizierapipb "pixielabs.ai/pixielabs/src/api/public/vizierapipb"
	"pixielabs.ai/pixielabs/src/cloud/api/ptproxy"
	"pixielabs.ai/pixielabs/src/cloud/autocomplete"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	pl_api_vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"

	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	"pixielabs.ai/pixielabs/src/cloud/shared/esutils"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/script"
	"pixielabs.ai/pixielabs/src/shared/services"
	svcEnv "pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/handler"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/server"
)

const defaultBundleFile = "https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-core.json"
const ossBundleFile = "https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle-oss.json"

func init() {
	pflag.String("domain_name", "dev.withpixie.dev", "The domain name of Pixie Cloud")
	pflag.String("nats_url", "pl-nats", "The URL of NATS")
	pflag.String("elastic_service", "https://pl-elastic-es-http.plc-dev.svc.cluster.local:9200", "The url of the elasticsearch cluster")
	pflag.String("elastic_ca_cert", "/elastic-certs/ca.crt", "CA Cert for elastic cluster")
	pflag.String("elastic_tls_cert", "/elastic-certs/tls.crt", "TLS Cert for elastic cluster")
	pflag.String("elastic_tls_key", "/elastic-certs/tls.key", "TLS Key for elastic cluster")
	pflag.String("elastic_username", "elastic", "Username for access to elastic cluster")
	pflag.String("elastic_password", "", "Password for access to elastic")
}

func main() {
	services.SetupService("api-service", 51200)
	services.SetupSSLClientFlags()
	vzshard.SetupFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	ac, err := controller.NewAuthClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init auth client")
	}

	// TODO(michelle): Move these to the controller server so that we can
	// deprecate the environment.

	pc, err := apienv.NewProfileServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init profile client")
	}

	vc, vk, err := apienv.NewVZMgrServiceClients()
	if err != nil {
		log.WithError(err).Fatal("Failed to init vzmgr clients")
	}

	at, err := apienv.NewArtifactTrackerClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init artifact tracker client")
	}

	ak, err := controller.NewAPIKeyClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init API key client")
	}

	env, err := apienv.New(ac, pc, vk, ak, vc, at)
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}

	// Connect to NATS.
	nc, err := nats.Connect(viper.GetString("nats_url"),
		nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
		nats.RootCAs(viper.GetString("tls_ca_cert")))
	if err != nil {
		log.WithError(err).Fatal("Could not connect to NATS")
	}

	nc.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, e error) {
		log.WithError(e).Error("Got NATS error")
	})

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
	mux.Handle("/api/auth/signup", handler.New(env, controller.AuthSignupHandler))
	mux.Handle("/api/auth/login", handler.New(env, controller.AuthLoginHandler))
	mux.Handle("/api/auth/logout", handler.New(env, controller.AuthLogoutHandler))
	// This is an unauthenticated path that will check and validate if a particular domain
	// is available for registration. This need to be unauthenticated because we need to check this before
	// the user registers.
	mux.Handle("/api/authorized", controller.WithAugmentedAuthMiddleware(env, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "OK")
	})))

	healthz.RegisterDefaultChecks(mux)

	// API service needs to convert any cookies into an augmented token in bearer auth.
	serverOpts := &server.GRPCServerOptions{
		AuthMiddleware: func(ctx context.Context, e svcEnv.Env) (string, error) {
			apiEnv, ok := e.(apienv.APIEnv)
			if !ok {
				return "", errors.New("Could not convert env to apiEnv")
			}
			return controller.GetAugmentedTokenGRPC(ctx, apiEnv)
		},
	}
	s := server.NewPLServerWithOptions(env, handlers.CORS(services.DefaultCORSConfig()...)(mux), serverOpts)

	imageAuthServer := &controller.VizierImageAuthServer{}
	cloudapipb.RegisterVizierImageAuthorizationServer(s.GRPCServer(), imageAuthServer)

	artifactTrackerServer := controller.ArtifactTrackerServer{
		ArtifactTrackerClient: at,
	}
	cloudapipb.RegisterArtifactTrackerServer(s.GRPCServer(), artifactTrackerServer)

	cis := &controller.VizierClusterInfo{VzMgr: vc, ArtifactTrackerClient: at}
	cloudapipb.RegisterVizierClusterInfoServer(s.GRPCServer(), cis)
	public_cloudapipb.RegisterClusterManagerServer(s.GRPCServer(), cis)

	vdks := &controller.VizierDeploymentKeyServer{VzDeploymentKey: vk}
	cloudapipb.RegisterVizierDeploymentKeyManagerServer(s.GRPCServer(), vdks)

	aks := &controller.APIKeyServer{APIKeyClient: ak}
	cloudapipb.RegisterAPIKeyManagerServer(s.GRPCServer(), aks)

	vpt := ptproxy.NewVizierPassThroughProxy(nc, vc)
	public_vizierapipb.RegisterVizierServiceServer(s.GRPCServer(), vpt)
	pl_api_vizierpb.RegisterVizierDebugServiceServer(s.GRPCServer(), vpt)

	sm, err := apienv.NewScriptMgrServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init scriptmgr client.")
	}
	sms := &controller.ScriptMgrServer{ScriptMgr: sm}
	cloudapipb.RegisterScriptMgrServer(s.GRPCServer(), sms)

	esSuggester, err := autocomplete.NewElasticSuggester(es, "scripts", pc)
	if err != nil {
		log.WithError(err).Fatal("Failed to start elastic suggester")
	}

	var br *script.BundleManager
	var bundleErr error
	updateBundle := func() {
		//TODO(michelle): Requiring the bundle manager in the API service is temporary until we
		// start indexing scripts.
		br, bundleErr = script.NewBundleManagerWithOrgName([]string{defaultBundleFile, ossBundleFile}, "")
		if bundleErr != nil {
			log.WithError(bundleErr).Error("Failed to init bundle manager")
			br = nil
		}
		err := esSuggester.UpdateScriptBundle(br)
		if err != nil {
			log.WithError(err).Error("Failed to update script bundle for suggester")
		}
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

	as := &controller.AutocompleteServer{Suggester: esSuggester}
	cloudapipb.RegisterAutocompleteServiceServer(s.GRPCServer(), as)

	profileServer := &controller.ProfileServer{pc}
	cloudapipb.RegisterProfileServiceServer(s.GRPCServer(), profileServer)

	gqlEnv := controller.GraphQLEnv{
		ArtifactTrackerServer: artifactTrackerServer,
		VizierClusterInfo:     cis,
		VizierDeployKeyMgr:    vdks,
		APIKeyMgr:             aks,
		ScriptMgrServer:       sms,
		AutocompleteServer:    as,
		ProfileServiceClient:  pc,
	}

	mux.Handle("/api/graphql", controller.WithAugmentedAuthMiddleware(env, controller.NewGraphQLHandler(gqlEnv)))

	mux.Handle("/api/unauthenticated/graphql", controller.NewUnauthenticatedGraphQLHandler(gqlEnv))

	s.Start()
	s.StopOnInterrupt()
}
