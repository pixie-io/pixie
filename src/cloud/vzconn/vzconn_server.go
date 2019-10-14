package main

import (
	"net/http"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/controller"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services/env"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

func init() {
	pflag.String("vzmgr_service", "kubernetes:///vzmgr-service.plc:51800", "The profile service url (load balancer/list is ok)")
}

// NewVZMgrServiceClient creates a new profile RPC client stub.
func NewVZMgrServiceClient() (vzmgrpb.VZMgrServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	vzmgrChannel, err := grpc.Dial(viper.GetString("vzmgr_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return vzmgrpb.NewVZMgrServiceClient(vzmgrChannel), nil
}

func main() {
	log.WithField("service", "vzconn-service").Info("Starting service")

	services.SetupService("vzconn-service", 51600)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)
	// VZConn is the backend for a GCLB and that health checks on "/" instead of the regular health check endpoint.
	healthz.InstallPathHandler(mux, "/")

	s := services.NewPLServer(env.New(), mux)

	vzmgrClient, err := NewVZMgrServiceClient()

	if err != nil {
		log.WithError(err).Fatal("failed to initialize vizer manager RPC client")
		panic(err)
	}
	server := controller.NewServer(vzmgrClient)
	vzconnpb.RegisterVZConnServiceServer(s.GRPCServer(), server)

	s.Start()
	s.StopOnInterrupt()
}
