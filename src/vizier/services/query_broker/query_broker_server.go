package main

import (
	"net/http"

	"github.com/nats-io/go-nats"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/services/common"
	"pixielabs.ai/pixielabs/src/services/common/healthz"
	"pixielabs.ai/pixielabs/src/services/common/httpmiddleware"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerenv"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

const plMDSAddr = "vizier-metadata.pl.svc.cluster.local:50400"

func main() {
	log.WithField("service", "query-broker").Info("Starting service")

	common.SetupService("query-broker", 50300)
	common.SetupGRPCClientFlags()
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.CheckGRPCClientFlags()
	common.SetupServiceLogging()

	env, err := querybrokerenv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}
	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	// Connect to metadata service.
	dialOpts, err := common.GetGRPCClientDialOpts()
	if err != nil {
		log.WithError(err).Fatal("Could not get dial opts.")
	}
	mdsConn, err := grpc.Dial(plMDSAddr, dialOpts...)
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to Metadata Service.")
	}
	mdsClient := metadatapb.NewMetadataServiceClient(mdsConn)

	// Connect to NATS.
	natsConn, err := nats.Connect("pl-nats")
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to NATS.")
	}

	server, err := controllers.NewServer(env, mdsClient, natsConn)
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
