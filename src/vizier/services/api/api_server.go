package main

import (
	"net/http"

	"github.com/gorilla/handlers"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"
	"pixielabs.ai/pixielabs/src/vizier/services/api/apienv"
	"pixielabs.ai/pixielabs/src/vizier/services/api/controller"
	"pixielabs.ai/pixielabs/src/vizier/services/shared/log/logwriter"
)

func main() {
	services.SetupService("api-service", 50200)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()

	err := logwriter.SetupLogger(viper.GetString("cloud_connector_addr"), viper.GetString("pod_name"), "api-service")
	if err != nil {
		log.WithError(err).Fatal("Could not connect to cloud connector for log forwarding")
	}
	log.WithField("service", "api-service").Info("Starting service")

	env, err := apienv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}
	h := http.Handler(controller.NewGraphQLHandler(env))
	mux := http.NewServeMux()
	mux.Handle("/graphql", h)

	healthz.RegisterDefaultChecks(mux)

	s := services.NewPLServer(env,
		handlers.CORS(services.DefaultCORSConfig()...)(
			httpmiddleware.WithBearerAuthMiddleware(env, mux)))
	s.Start()
	s.StopOnInterrupt()
}
