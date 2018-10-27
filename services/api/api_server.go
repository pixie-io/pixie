package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/services/api/controller"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/services/common/healthz"
)

func main() {
	log.WithField("service", "api-service").Info("Starting service")

	common.SetupService("api-service", 50200)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	mux := http.NewServeMux()
	mux.Handle("/gql", controller.GraphQLHandler())
	healthz.RegisterDefaultChecks(mux)

	s := common.NewPLServer(nil, mux)
	s.Start()
	s.StopOnInterrupt()
}
