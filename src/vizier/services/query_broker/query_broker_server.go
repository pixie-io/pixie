package main

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"time"

	"github.com/cenkalti/backoff/v3"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	public_vizierapipb "pixielabs.ai/pixielabs/src/api/public/vizierapipb"
	"pixielabs.ai/pixielabs/src/carnotpb"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"
	"pixielabs.ai/pixielabs/src/shared/services/server"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/ptproxy"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerenv"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/tracker"
)

const (
	plMDSAddr           = "vizier-metadata.pl.svc:50400"
	querybrokerHostname = "vizier-query-broker.pl.svc"
)

func init() {
	pflag.String("cloud_connector_addr", "vizier-cloud-connector.pl.svc:50800", "The address to the cloud connector")
	pflag.String("cluster_id", "", "The Cluster ID to use for Pixie Cloud")
	pflag.String("pod_ip_address", "", "The IP address of this pod to allow the agent to connect to this"+
		" particular query broker instance across multiple requests")
}

// NewVizierServiceClient creates a new vz RPC client stub.
func NewVizierServiceClient(port uint) (public_vizierapipb.VizierServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	// Note: This has to be localhost to pass the SSL cert verification.
	addr := fmt.Sprintf("localhost:%d", port)
	vzChannel, err := grpc.Dial(addr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return public_vizierapipb.NewVizierServiceClient(vzChannel), nil
}

func main() {
	servicePort := uint(50300)
	services.SetupService("query-broker", servicePort)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	flush := services.InitDefaultSentry(viper.GetString("cluster_id"))
	defer flush()

	// For a given query, the agents may send multiple sets of results via the query broker.
	// We pass the pod IP address down to the agents in order to ensure that they continue to
	// communicate with the same query broker across a single request.
	// TODO fix to ip
	podAddr := viper.GetString("pod_ip_address")
	if podAddr == "" {
		log.Fatal("Expected to receive pod IP address.")
	}
	env, err := querybrokerenv.New(fmt.Sprintf("%s:%d", podAddr, servicePort), querybrokerHostname)
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment.")
	}
	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	// Connect to metadata service.
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		log.WithError(err).Fatal("Could not get dial opts.")
	}
	dialOpts = append(dialOpts, grpc.WithBlock())
	dialOpts = append(dialOpts, grpc.WithDefaultCallOptions(grpc.MaxCallRecvMsgSize(8*1024*1024)))

	bOpts := backoff.NewExponentialBackOff()
	bOpts.InitialInterval = 15 * time.Second
	bOpts.MaxElapsedTime = 5 * time.Minute

	var mdsConn *grpc.ClientConn
	err = backoff.Retry(func() error {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()
		mdsConn, err = grpc.DialContext(ctx, plMDSAddr, dialOpts...)
		if !errors.Is(err, context.DeadlineExceeded) {
			// Any errors that aren't timeouts are treated as permanent errors.
			return backoff.Permanent(err)
		}
		return err
	}, bOpts)

	if err != nil {
		log.WithError(err).Fatal("Failed to connect to Metadata Service.")
	}

	defer mdsConn.Close()

	mdsClient := metadatapb.NewMetadataServiceClient(mdsConn)
	mdtpClient := metadatapb.NewMetadataTracepointServiceClient(mdsConn)
	mdconfClient := metadatapb.NewMetadataConfigServiceClient(mdsConn)

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
	natsConn.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		log.WithError(err).
			WithField("sub", subscription.Subject).
			Error("Got nats error")
	})

	agentTracker := tracker.NewAgents(mdsClient, viper.GetString("jwt_signing_key"))
	agentTracker.Start()
	defer agentTracker.Stop()
	svr, err := controllers.NewServer(env, agentTracker, mdtpClient, mdconfClient, natsConn)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs.")
	}
	defer svr.Close()

	// For query broker we bump up the max message size since resuls might be larger than 4mb.
	maxMsgSize := grpc.MaxRecvMsgSize(8 * 1024 * 1024)

	s := server.NewPLServer(env,
		httpmiddleware.WithBearerAuthMiddleware(env, mux), maxMsgSize)

	carnotpb.RegisterResultSinkServiceServer(s.GRPCServer(), svr)
	public_vizierapipb.RegisterVizierServiceServer(s.GRPCServer(), svr)

	// For the passthrough proxy we create a GRPC client to the current server. It appears really
	// hard to emulate the streaming GRPC connection and this helps keep the API straightforward.
	vzServiceClient, err := NewVizierServiceClient(servicePort)
	if err != nil {
		log.WithError(err).Fatal("Failed to init vzservice client.")
	}

	// Start passthrough proxy.
	ptProxy, err := ptproxy.NewPassThroughProxy(natsConn, vzServiceClient)
	if err != nil {
		log.WithError(err).Fatal("Failed to start passthrough proxy.")
	}
	go ptProxy.Run()
	defer ptProxy.Close()

	s.Start()
	s.StopOnInterrupt()
}
