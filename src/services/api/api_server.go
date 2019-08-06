package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/services/api/apienv"
	"pixielabs.ai/pixielabs/src/services/api/controller"
	"pixielabs.ai/pixielabs/src/services/common"
	"pixielabs.ai/pixielabs/src/services/common/healthz"
	"pixielabs.ai/pixielabs/src/services/common/httpmiddleware"
)

func main() {
	log.WithField("service", "api-service(cloud)").Info("Starting service")

	common.SetupService("api-service", 51200)
	common.SetupSSLClientFlags()
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.CheckSSLClientFlags()
	common.SetupServiceLogging()

	env, err := apienv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}
	h := http.Handler(controller.NewGraphQLHandler(env))
	mux := http.NewServeMux()
	mux.Handle("/graphql", h)

	healthz.RegisterDefaultChecks(mux)

	s := common.NewPLServer(env,
		httpmiddleware.WithNewSessionMiddleware(
			httpmiddleware.WithBearerAuthMiddleware(env, mux)))
	s.Start()
	s.StopOnInterrupt()
}
