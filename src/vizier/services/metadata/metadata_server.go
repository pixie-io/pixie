package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/services/common"
	"pixielabs.ai/pixielabs/src/services/common/healthz"
	"pixielabs.ai/pixielabs/src/services/common/httpmiddleware"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadataenv"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
)

func main() {
	log.WithField("service", "metadata").Info("Starting service")

	common.SetupService("metadata", 50400)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	mc, err := controllers.NewMessageBusController("pl-nats", "update_agent")
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to message bus")
	}
	defer mc.Close()

	env, err := metadataenv.New()
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
	metadatapb.RegisterMetadataServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
