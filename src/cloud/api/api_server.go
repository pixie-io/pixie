package main

import (
	"context"
	"errors"
	"fmt"
	"net/http"

	"github.com/nats-io/nats.go"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/cloud/api/ptproxy"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	pl_api_vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"

	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	"pixielabs.ai/pixielabs/src/shared/services"
	svcEnv "pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/handler"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

func init() {
	pflag.String("domain_name", "dev.withpixie.dev", "The domain name of Pixie Cloud")
	pflag.String("nats_url", "pl-nats", "The URL of NATS")
}

func main() {
	log.WithField("service", "api-service(cloud)").Info("Starting service")

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

	vc, err := apienv.NewVZMgrServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init vzmgr client")
	}

	at, err := apienv.NewArtifactTrackerClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init artifact tracker client")
	}

	env, err := apienv.New(ac, pc, vc, at)
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
	serverOpts := &services.GRPCServerOptions{
		AuthMiddleware: func(ctx context.Context, e svcEnv.Env) (string, error) {
			apiEnv, ok := e.(apienv.APIEnv)
			if !ok {
				return "", errors.New("Could not convert env to apiEnv")
			}
			return controller.GetAugmentedTokenGRPC(ctx, apiEnv)
		},
	}
	s := services.NewPLServerWithOptions(env, mux, serverOpts)

	imageAuthServer := &controller.VizierImageAuthServer{}
	cloudapipb.RegisterVizierImageAuthorizationServer(s.GRPCServer(), imageAuthServer)

	artifactTrackerServer := controller.ArtifactTrackerServer{
		ArtifactTrackerClient: at,
	}
	cloudapipb.RegisterArtifactTrackerServer(s.GRPCServer(), artifactTrackerServer)

	cis := &controller.VizierClusterInfo{VzMgr: vc}
	cloudapipb.RegisterVizierClusterInfoServer(s.GRPCServer(), cis)

	vpt := ptproxy.NewVizierPassThroughProxy(nc, vc)
	pl_api_vizierpb.RegisterVizierServiceServer(s.GRPCServer(), vpt)

	gqlEnv := controller.GraphQLEnv{
		ArtifactTrackerServer: artifactTrackerServer,
		VizierClusterInfo:     cis,
	}

	mux.Handle("/api/graphql", controller.WithAugmentedAuthMiddleware(env, controller.NewGraphQLHandler(gqlEnv)))

	s.Start()
	s.StopOnInterrupt()
}
