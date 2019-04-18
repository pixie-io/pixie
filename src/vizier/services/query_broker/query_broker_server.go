package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/services/common"
	"pixielabs.ai/pixielabs/src/services/common/healthz"
	"pixielabs.ai/pixielabs/src/services/common/httpmiddleware"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/env"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

func main() {
	log.WithField("service", "query-broker").Info("Starting service")

	common.SetupService("query-broker", 50300)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	env, err := querybrokerenv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}
	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	server, err := controllers.NewServer(env)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs")
	}

	s := common.NewPLServer(env,
		httpmiddleware.WithNewSessionMiddleware(
			httpmiddleware.WithBearerAuthMiddleware(env, mux)))
	querybrokerpb.RegisterQueryBrokerServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
