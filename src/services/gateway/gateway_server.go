package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/services/common"
	"pixielabs.ai/pixielabs/src/services/common/handler"
	"pixielabs.ai/pixielabs/src/services/common/healthz"
	"pixielabs.ai/pixielabs/src/services/common/httpmiddleware"
	"pixielabs.ai/pixielabs/src/services/gateway/controllers"
	"pixielabs.ai/pixielabs/src/services/gateway/gwenv"
)

func main() {
	log.WithField("service", "gateway-service").Info("Starting service")

	common.SetupService("gateway-service", 51000)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	ac, err := controllers.NewAuthClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init auth client")
	}
	gwEnv, err := gwenv.New(ac)
	if err != nil {
		log.WithError(err).Fatal(
			"Failed to initialize gateway error")
	}

	mux := http.NewServeMux()
	mux.Handle("/api/auth/login", handler.New(gwEnv, controllers.AuthLoginHandler))
	mux.Handle("/api/auth/logout", handler.New(gwEnv, controllers.AuthLogoutHandler))
	mux.Handle("/api/graphql",
		httpmiddleware.WithNewSessionMiddleware(
			controllers.WithSessionAuthMiddleware(gwEnv,
				controllers.WithAugmentedAuthMiddleware(gwEnv,
					handler.New(gwEnv, controllers.GraphQLHandler)))))

	healthz.RegisterDefaultChecks(mux)
	s := common.NewPLServer(gwEnv, mux)
	s.Start()
	s.StopOnInterrupt()
}
