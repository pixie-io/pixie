package main

import (
	"context"
	"net/http"
	"time"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	k8sErrors "k8s.io/apimachinery/pkg/api/errors"

	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/election"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"
	"pixielabs.ai/pixielabs/src/shared/version"
	controllers "pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/bridge"
	"pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/vizhealth"
	pl_api_vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

func init() {
	pflag.String("cluster_id", "", "The Cluster ID to use for Pixie Cloud")
	pflag.String("certmgr_service", "vizier-certmgr.pl.svc:50900", "The cert manager service url (load balancer/list is ok)")
	pflag.String("nats_url", "pl-nats", "The URL of NATS")
	pflag.Duration("max_expected_clock_skew", 750, "Duration in ms of expected maximum clock skew in a cluster")
	pflag.Duration("renew_period", 500, "Duration in ms of the time to wait to renew lease")
	pflag.String("pod_namespace", "pl", "The namespace this pod runs in. Used for leader elections")
	pflag.String("qb_service", "vizier-query-broker.pl.svc:50300", "The querybroker service url (load balancer/list is ok)")
	pflag.String("cluster_name", "", "The name of the user's K8s cluster")
	pflag.String("deploy_key", "", "The deploy key for the cluster")
}
func newVzServiceClient() (pl_api_vizierpb.VizierServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	qbChannel, err := grpc.Dial(viper.GetString("qb_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return pl_api_vizierpb.NewVizierServiceClient(qbChannel), nil
}

func main() {
	log.WithField("service", "cloud-connector").
		WithField("version", version.GetVersion().ToString()).
		Info("Starting service")

	services.SetupService("cloud-connector", 50800)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	flush := services.InitDefaultSentry(viper.GetString("cluster_id"))
	defer flush()

	clusterID := viper.GetString("cluster_id")
	vizierID := uuid.FromStringOrNil(clusterID)

	deployKey := viper.GetString("deploy_key")

	vzInfo, err := controllers.NewK8sVizierInfo(viper.GetString("cluster_name"))
	if err != nil {
		log.WithError(err).Fatal("Could not get k8s info")
	}

	// Clean up cert-provisioner-job, if exists.
	certJob, err := vzInfo.GetJob("cert-provisioner-job")
	if certJob != nil {
		err = vzInfo.DeleteJob("cert-provisioner-job")
		if err != nil && !k8sErrors.IsNotFound(err) {
			log.WithError(err).Info("Error deleting cert-provisioner-job")
		}
	}

	leaderMgr, err := election.NewK8sLeaderElectionMgr(
		viper.GetString("pod_namespace"),
		viper.GetDuration("max_expected_clock_skew"),
		viper.GetDuration("renew_period"),
		"cloud-conn-election",
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

	qbVzClient, err := newVzServiceClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to init qb stub")
	}
	checker := vizhealth.NewChecker(viper.GetString("jwt_signing_key"), qbVzClient)
	defer checker.Stop()

	// Periodically clean up any completed jobs.
	quitCh := make(chan bool)
	go vzInfo.CleanupCronJob("etcd-defrag-job", 2*time.Hour, quitCh)
	defer close(quitCh)

	// We just use the current time in nanoseconds to mark the session ID. This will let the cloud side know that
	// the cloud connector restarted. Clock skew might make this incorrect, but we mostly want this for debugging.
	sessionID := time.Now().UnixNano()
	server := controllers.New(vizierID, viper.GetString("jwt_signing_key"), deployKey, sessionID, nil, vzInfo, nil, checker)
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
