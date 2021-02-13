package main

import (
	"net/http"

	"github.com/golang-migrate/migrate"
	"github.com/golang-migrate/migrate/database/postgres"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	dnsmgrpb "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/controller"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/deployment"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/deploymentkey"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/schema"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/msgbus"
	"pixielabs.ai/pixielabs/src/shared/services/pg"

	log "github.com/sirupsen/logrus"
	artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
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
	healthz.RegisterDefaultChecks(mux)

	s := server.NewPLServer(env.New(), mux)

	dnsMgrClient, err := NewDNSMgrServiceClient()
	if err != nil {
		log.WithError(err).Fatal("failed to initialize DNS manager RPC client")
		panic(err)
	}

	db := pg.MustConnectDefaultPostgresDB()

	// TODO(zasgar): Pull out this migration code into a util. Just leaving it here for now for testing.
	driver, err := postgres.WithInstance(db.DB, &postgres.Config{
		MigrationsTable: "vzmgr_service_migrations",
	})

	sc := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})

	d, err := bindata.WithInstance(sc)

	mg, err := migrate.NewWithInstance(
		"go-bindata",
		d, "postgres", driver)

	if err = mg.Up(); err != nil {
		log.WithError(err).Info("migrations failed: %s", err)
	}

	dbKey := viper.GetString("database_key")
	if dbKey == "" {
		log.Fatal("Database encryption key is required")
	}

	// Connect to NATS.
	nc := msgbus.MustConnectNATS()
	stc := msgbus.MustConnectSTAN(nc, uuid.NewV4().String())

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

	_, err = controller.NewMetadataReader(db, stc, nc)
	if err != nil {
		log.WithError(err).Fatal("Could not start metadata listener")
	}

	s.Start()
	s.StopOnInterrupt()
}
