package main

import (
	"net/http"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"
	controllers "pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/controller"
)

func init() {
	pflag.String("cluster_id", "", "The Cluster ID to use for Pixie Cloud")
}

func main() {
	log.WithField("service", "cloud-connector").Info("Starting service")

	services.SetupService("cloud-connector", 50800)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	vzClient, err := controllers.NewVZConnClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init vzconn client")
	}

	clusterID := viper.GetString("cluster_id")
	if clusterID == "" {
		log.Fatal("Cluster ID is required")
	}
	vizierID, err := uuid.FromString(clusterID)
	if err != nil {
		log.WithError(err).Fatal("Could not parse cluster_id")
	}

	server := controllers.NewServer(vizierID, viper.GetString("jwt_signing_key"), vzClient)
	server.StartStream()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	// Connect to metadata service.
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		log.WithError(err).Fatal("Could not get dial opts.")
	}
	dialOpts = append(dialOpts, grpc.WithBlock())
	e := env.New()
	s := services.NewPLServer(e,
		httpmiddleware.WithBearerAuthMiddleware(e, mux))
	s.Start()
	s.StopOnInterrupt()
}
