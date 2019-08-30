package main

import (
	"context"
	"net/http"
	"time"

	"github.com/nats-io/go-nats"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/carnot/compiler"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerenv"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

const plMDSAddr = "vizier-metadata.pl.svc.cluster.local:50400"

func main() {
	log.WithField("service", "query-broker").Info("Starting service")

	services.SetupService("query-broker", 50300)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	env, err := querybrokerenv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}
	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	// Connect to metadata service.
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		log.WithError(err).Fatal("Could not get dial opts.")
	}
	dialOpts = append(dialOpts, grpc.WithBlock())

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	mdsConn, err := grpc.DialContext(ctx, plMDSAddr, dialOpts...)
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to Metadata Service.")
	}
	mdsClient := metadatapb.NewMetadataServiceClient(mdsConn)

	// Connect to NATS.
	var natsConn *nats.Conn
	if viper.GetBool("disable_ssl") {
		natsConn, err = nats.Connect("pl-nats")
	} else {
		natsConn, err = nats.Connect("pl-nats",
			nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
			nats.RootCAs(viper.GetString("tls_ca_cert")))
	}
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to NATS.")
	}
	compilerPtr := compiler.New()
	defer compilerPtr.Free()
	server, err := controllers.NewServer(env, mdsClient, natsConn, compilerPtr)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs")
	}

	s := services.NewPLServer(env,
		httpmiddleware.WithBearerAuthMiddleware(env, mux))
	querybrokerpb.RegisterQueryBrokerServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
