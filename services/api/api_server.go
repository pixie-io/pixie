package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/services/api/apienv"
	"pixielabs.ai/pixielabs/services/api/controller"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/services/common/healthz"
	"pixielabs.ai/pixielabs/services/common/httpmiddleware"
)

func main() {
	log.WithField("service", "api-service").Info("Starting service")

	common.SetupService("api-service", 50200)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	env, err := apienv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}
	h := http.Handler(controller.GraphQLHandler(env))
	mux := http.NewServeMux()
	mux.Handle("/graphql", h)

	healthz.RegisterDefaultChecks(mux)

	s := common.NewPLServer(env,
		httpmiddleware.WithNewSessionMiddleware(
			httpmiddleware.WithBearerAuthMiddleware(env, mux)))
	s.Start()
	s.StopOnInterrupt()
}
