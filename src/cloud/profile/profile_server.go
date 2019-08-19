package main

import (
	"net/http"

	profile "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

func main() {
	log.WithField("service", "profile-service").Info("Starting service")

	services.SetupService("profile-service", 51500)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	s := services.NewPLServer(nil, mux)
	profile.RegisterProfileServiceServer(s.GRPCServer(), nil)
	s.Start()
	s.StopOnInterrupt()
}
