package main

import (
	"net/http"

	"pixielabs.ai/pixielabs/src/shared/services/env"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/cloud/autocomplete/autocompletepb"
	"pixielabs.ai/pixielabs/src/cloud/autocomplete/controller"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

func main() {
	log.WithField("service", "autocomplete-service").Info("Starting service")

	services.SetupService("autocomplete-service", 52100)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	s := services.NewPLServer(env.New(), mux)

	server := controller.NewServer()
	autocompletepb.RegisterAutocompleteServiceServer(s.GRPCServer(), server)

	s.Start()
	s.StopOnInterrupt()
}
