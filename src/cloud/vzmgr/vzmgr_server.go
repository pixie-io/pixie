package main

import (
	"net/http"

	"pixielabs.ai/pixielabs/src/cloud/vzmgr/controller"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"

	"pixielabs.ai/pixielabs/src/shared/services/env"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

func main() {
	log.WithField("service", "vzmgr-service").Info("Starting service")

	services.SetupService("vzmgr-service", 51500)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	s := services.NewPLServer(env.New(), mux)

	c := controller.New(nil)
	vzmgrpb.RegisterVZMgrServiceServer(s.GRPCServer(), c)

	s.Start()
	s.StopOnInterrupt()
}
