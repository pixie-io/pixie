package main

import (
	"fmt"
	"net/http"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/handler"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

func init() {
	pflag.String("domain_name", "dev.withpixie.dev", "The domain name of Pixie Cloud")
}

func main() {
	log.WithField("service", "api-service(cloud)").Info("Starting service")

	services.SetupService("api-service", 51200)
	services.SetupSSLClientFlags()
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
	sc, err := apienv.NewSiteManagerServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init site manager client")
	}

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

	env, err := apienv.New(ac, sc, pc, vc, at)
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}

	csh := controller.NewCheckSiteHandler(env)
	mux := http.NewServeMux()
	mux.Handle("/api/create-site", handler.New(env, controller.CreateSiteHandler))
	mux.Handle("/api/auth/login", handler.New(env, controller.AuthLoginHandler))
	mux.Handle("/api/auth/logout", handler.New(env, controller.AuthLogoutHandler))
	// This is an unauthenticated path that will check and validate if a particular domain
	// is available for registration. This need to be unauthenticated because we need to check this before
	// the user registers.
	mux.Handle("/api/site/check", http.HandlerFunc(csh.HandlerFunc))
	mux.Handle("/api/authorized", controller.WithAugmentedAuthMiddleware(env, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "OK")
	})))
	mux.Handle("/api/graphql",
		controller.WithAugmentedAuthMiddleware(env,
			controller.NewGraphQLHandler(env)))

	healthz.RegisterDefaultChecks(mux)
	s := services.NewPLServer(env, mux)
	imageAuthServer := &controller.VizierImageAuthSever{}
	cloudapipb.RegisterVizierImageAuthorizationServer(s.GRPCServer(), imageAuthServer)

	s.Start()
	s.StopOnInterrupt()
}
