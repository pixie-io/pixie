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

	env, err := apienv.New(ac)
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}

	mux := http.NewServeMux()
	mux.Handle("/api/auth/login", handler.New(env, controller.AuthLoginHandler))
	mux.Handle("/api/auth/logout", handler.New(env, controller.AuthLogoutHandler))
	mux.Handle("/api/graphql",
		controller.WithAugmentedAuthMiddleware(env,
			controller.NewGraphQLHandler(env)))

	healthz.RegisterDefaultChecks(mux)
	s := common.NewPLServer(env, mux)
	s.Start()
	s.StopOnInterrupt()
}
