package main

import (
	"net/http"
	_ "net/http/pprof"

	"pixielabs.ai/pixielabs/src/cloud/dnsmgr/schema"
	"pixielabs.ai/pixielabs/src/cloud/shared/pgmigrate"

	"github.com/gofrs/uuid"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	dnsmgrpb "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/controller"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/deployment"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/deploymentkey"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/msgbus"
	"pixielabs.ai/pixielabs/src/shared/services/pg"
	"pixielabs.ai/pixielabs/src/shared/services/server"
)

func init() {
	pflag.String("database_key", "", "The encryption key to use for the database")
	pflag.String("dnsmgr_service", "dnsmgr-service.plc.svc.cluster.local:51900", "The dns manager service url (load balancer/list is ok)")
}

// NewDNSMgrServiceClient creates a new profile RPC client stub.
func NewDNSMgrServiceClient() (dnsmgrpb.DNSMgrServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	dnsMgrChannel, err := grpc.Dial(viper.GetString("dnsmgr_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return dnsmgrpb.NewDNSMgrServiceClient(dnsMgrChannel), nil
}

// NewArtifactTrackerServiceClient creates a new artifact tracker RPC client stub.
func NewArtifactTrackerServiceClient() (artifacttrackerpb.ArtifactTrackerClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	atChannel, err := grpc.Dial(viper.GetString("artifact_tracker_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return artifacttrackerpb.NewArtifactTrackerClient(atChannel), nil
}

func main() {
	services.SetupService("vzmgr-service", 51800)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	s := server.NewPLServer(env.New(), mux)

	dnsMgrClient, err := NewDNSMgrServiceClient()
	if err != nil {
		log.WithError(err).Fatal("failed to initialize DNS manager RPC client")
		panic(err)
	}

	db := pg.MustConnectDefaultPostgresDB()
	err = pgmigrate.PerformMigrationsUsingBindata(db, "vzmgr_service_migrations", &pgmigrate.SchemaAssetFetcher{
		AssetNames: schema.AssetNames,
		Asset:      schema.Asset,
	})
	if err != nil {
		log.WithError(err).Fatal("Failed to apply migrations")
	}

	dbKey := viper.GetString("database_key")
	if dbKey == "" {
		log.Fatal("Database encryption key is required")
	}

	// Connect to NATS.
	nc := msgbus.MustConnectNATS()
	stc := msgbus.MustConnectSTAN(nc, uuid.Must(uuid.NewV4()).String())

	nc.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		if err != nil {
			log.WithError(err).
				WithField("Subject", subscription.Subject).
				Error("Got NATS error")
		}
	})

	at, err := NewArtifactTrackerServiceClient()
	if err != nil {
		log.Fatal("Could not connect to artifact tracker")
	}

	updater, err := controller.NewUpdater(db, at, nc)
	if err != nil {
		log.WithError(err).Fatal("Could not start vizier updater")
	}
	go updater.ProcessUpdateQueue()
	defer updater.Stop()

	c := controller.New(db, dbKey, dnsMgrClient, nc, updater)
	dks := deploymentkey.New(db, dbKey)
	ds := deployment.New(dks, c)

	sm := controller.NewStatusMonitor(db)
	defer sm.Stop()
	vzmgrpb.RegisterVZMgrServiceServer(s.GRPCServer(), c)
	vzmgrpb.RegisterVZDeploymentKeyServiceServer(s.GRPCServer(), dks)
	vzmgrpb.RegisterVZDeploymentServiceServer(s.GRPCServer(), ds)

	mdr, err := controller.NewMetadataReader(db, stc, nc)
	if err != nil {
		log.WithError(err).Fatal("Could not start metadata listener")
	}
	defer mdr.Stop()

	s.Start()
	s.StopOnInterrupt()
}
