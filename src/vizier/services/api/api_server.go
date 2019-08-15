package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"
	"pixielabs.ai/pixielabs/src/vizier/services/api/apienv"
	"pixielabs.ai/pixielabs/src/vizier/services/api/controller"
)

func main() {
	log.WithField("service", "api-service").Info("Starting service")

	services.SetupService("api-service", 50200)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	env, err := apienv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}
	h := http.Handler(controller.NewGraphQLHandler(env))
	mux := http.NewServeMux()
	mux.Handle("/graphql", h)

	healthz.RegisterDefaultChecks(mux)

	s := services.NewPLServer(env,
		httpmiddleware.WithBearerAuthMiddleware(env, mux))
	s.Start()
	s.StopOnInterrupt()
}
