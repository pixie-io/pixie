package main

import (
	"net/http"

	"github.com/gofrs/uuid"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/server"
	"pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrenv"
	certmgrpb "pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrpb"
	"pixielabs.ai/pixielabs/src/vizier/services/certmgr/controller"
)

func init() {
	pflag.String("namespace", "pl", "The namespace of Vizier")
	pflag.String("cluster_id", "", "The Cluster ID to use for Pixie Cloud")
	pflag.String("nats_url", "pl-nats", "The URL of NATS")
}

func main() {
	services.SetupService("certmgr-service", 50900)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	flush := services.InitDefaultSentry(viper.GetString("cluster_id"))
	defer flush()

	nc, err := nats.Connect(viper.GetString("nats_url"),
		nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
		nats.RootCAs(viper.GetString("tls_ca_cert")))
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to NATS.")
	}

	clusterID, err := uuid.FromString(viper.GetString("cluster_id"))
	if err != nil {
		log.WithError(err).Fatal("Failed to parse passed in cluster ID")
	}

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	k8sAPI, err := controller.NewK8sAPI(viper.GetString("namespace"))
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to K8S API")
	}

	env := certmgrenv.New()
	svr := controller.NewServer(env, clusterID, nc, k8sAPI)
	go svr.CertRequester()
	defer svr.StopCertRequester()

	s := server.NewPLServer(env, mux)
	certmgrpb.RegisterCertMgrServiceServer(s.GRPCServer(), svr)
	s.Start()
	s.StopOnInterrupt()
}
