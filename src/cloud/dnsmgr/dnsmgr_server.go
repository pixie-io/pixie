package main

import (
	"net/http"

	"pixielabs.ai/pixielabs/src/cloud/dnsmgr/controller"
	"pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrenv"
	dnsmgrpb "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

func main() {
	log.WithField("service", "dnsmgr-service").Info("Starting service")

	services.SetupService("dnsmgr-service", 51900)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	env := dnsmgrenv.New()
	server := controller.NewServer(env)

	s := services.NewPLServer(env, mux)
	dnsmgrpb.RegisterDNSMgrServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
