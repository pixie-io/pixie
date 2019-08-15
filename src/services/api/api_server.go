package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/services/api/apienv"
	"pixielabs.ai/pixielabs/src/services/api/controller"
	"pixielabs.ai/pixielabs/src/services/common"
	"pixielabs.ai/pixielabs/src/services/common/handler"
	"pixielabs.ai/pixielabs/src/services/common/healthz"
)

func main() {
	log.WithField("service", "api-service(cloud)").Info("Starting service")

	common.SetupService("api-service", 51200)
	common.SetupSSLClientFlags()
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.CheckSSLClientFlags()
	common.SetupServiceLogging()

	ac, err := controller.NewAuthClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init auth client")
	}

	sc, err := apienv.NewSiteManagerServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init site manager client")
	}

	env, err := apienv.New(ac, sc)
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}

	csh := controller.NewCheckSiteHandler(env)
	mux := http.NewServeMux()
	mux.Handle("/api/auth/login", handler.New(env, controller.AuthLoginHandler))
	mux.Handle("/api/auth/logout", handler.New(env, controller.AuthLogoutHandler))
	// This is an unauthenticated path that will check and validate if a particular domain
	// is available for registration. This need to be unauthenticated because we need to check this before
	// the user registers.
	mux.Handle("/api/site/check", http.HandlerFunc(csh.HandlerFunc))
	mux.Handle("/api/graphql",
		controller.WithAugmentedAuthMiddleware(env,
			controller.NewGraphQLHandler(env)))

	healthz.RegisterDefaultChecks(mux)
	s := common.NewPLServer(env, mux)
	s.Start()
	s.StopOnInterrupt()
}
