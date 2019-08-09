package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/services/common"
	"pixielabs.ai/pixielabs/src/services/common/healthz"
	controllers "pixielabs.ai/pixielabs/src/services/site_manager/controller"
	"pixielabs.ai/pixielabs/src/services/site_manager/sitemanagerenv"
	"pixielabs.ai/pixielabs/src/services/site_manager/sitemanagerpb"
)

func main() {
	log.WithField("service", "site-manager-service").Info("Starting service")

	common.SetupService("site-manager-service", 50300)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	env, err := sitemanagerenv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize env")
	}

	server, err := controllers.NewServer(env)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs")
	}

	s := common.NewPLServer(env, mux)
	sitemanagerpb.RegisterSiteManagerServiceServer(s.GRPCServer(), server)

	s.Start()
	s.StopOnInterrupt()
}
