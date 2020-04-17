package main

import (
	"context"
	"net/http"
	"time"

	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/election"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"
	certmgrpb "pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrpb"
	controllers "pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/bridge"
	"pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/ptproxy"
	vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

func init() {
	pflag.String("cluster_id", "", "The Cluster ID to use for Pixie Cloud")
	pflag.String("certmgr_service", "vizier-certmgr.pl.svc:50900", "The cert manager service url (load balancer/list is ok)")
	pflag.String("nats_url", "pl-nats", "The URL of NATS")
	pflag.Duration("max_expected_clock_skew", 750, "Duration in ms of expected maximum clock skew in a cluster")
	pflag.Duration("renew_period", 500, "Duration in ms of the time to wait to renew lease")
	pflag.String("pod_namespace", "pl", "The namespace this pod runs in. Used for leader elections")
	pflag.String("qb_service", "vizier-query-broker.pl.svc:50300", "The querybroker service url (load balancer/list is ok)")
}

// NewCertMgrServiceClient creates a new cert mgr RPC client stub.
func NewCertMgrServiceClient() (certmgrpb.CertMgrServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	// Block until we connect to prevent races.
	dialOpts = append(dialOpts, grpc.WithBlock())

	certMgrChannel, err := grpc.Dial(viper.GetString("certmgr_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return certmgrpb.NewCertMgrServiceClient(certMgrChannel), nil
}

// NewVizierServiceClient creates a new vz RPC client stub.
func NewVizierServiceClient() (vizierpb.VizierServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	// Block until we connect to prevent races.
	dialOpts = append(dialOpts, grpc.WithBlock())

	vzChannel, err := grpc.Dial(viper.GetString("qb_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return vizierpb.NewVizierServiceClient(vzChannel), nil
}

func main() {
	log.WithField("service", "cloud-connector").Info("Starting service")

	services.SetupService("cloud-connector", 50800)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	flush := services.InitDefaultSentry(viper.GetString("cluster_id"))
	defer flush()

	vzClient, err := controllers.NewVZConnClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init vzconn client")
	}

	certMgrClient, err := NewCertMgrServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init certmgr client")
	}

	vzServiceClient, err := NewVizierServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init vzservice client")
	}

	clusterID := viper.GetString("cluster_id")
	if clusterID == "" {
		log.Fatal("Cluster ID is required")
	}
	vizierID, err := uuid.FromString(clusterID)
	if err != nil {
		log.WithError(err).Fatal("Could not parse cluster_id")
	}

	vzInfo, err := controllers.NewK8sVizierInfo()
	if err != nil {
		log.WithError(err).Fatal("Could not get k8s info")
	}

	nc, err := nats.Connect(viper.GetString("nats_url"),
		nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
		nats.RootCAs(viper.GetString("tls_ca_cert")))
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to NATS.")
	}

	nc.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		log.WithField("Sub", subscription.Subject).
			WithError(err).
			Error("Error with NATS handler")
	})

	leaderMgr, err := election.NewK8sLeaderElectionMgr(
		viper.GetString("pod_namespace"),
		viper.GetDuration("max_expected_clock_skew"),
		viper.GetDuration("renew_period"),
	)

	if err != nil {
		log.WithError(err).Fatal("Failed to connect to leader election manager.")
	}
	// Cancel callback causes leader to resign.
	leaderCtx, cancel := context.WithCancel(context.Background())
	err = leaderMgr.Campaign(leaderCtx)
	if err != nil {
		log.WithError(err).Fatal("Failed to become leader")
	}

	resign := func() {
		log.Info("Resigning leadership")
		cancel()
	}
	// Resign leadership after the server stops.
	defer resign()

	// Start passthrough proxy.
	ptProxy, err := ptproxy.NewPassThroughProxy(nc, vzServiceClient)
	if err != nil {
		log.WithError(err).Fatal("Failed to start passthrough proxy.")
	}
	go ptProxy.Run()
	defer ptProxy.Close()

	// We just use the current time in nanoseconds to mark the session ID. This will let the cloud side know that
	// the cloud connector restarted. Clock skew might make this incorrect, but we mostly want this for debugging.
	sessionID := time.Now().UnixNano()
	server := controllers.New(vizierID, viper.GetString("jwt_signing_key"), sessionID, vzClient, certMgrClient, vzInfo, nc)
	go server.RunStream()
	defer server.Stop()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	e := env.New()
	s := services.NewPLServer(e,
		httpmiddleware.WithBearerAuthMiddleware(e, mux))

	s.Start()

	s.StopOnInterrupt()
}
