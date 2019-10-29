package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrenv"
	certmgrpb "pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrpb"
	"pixielabs.ai/pixielabs/src/vizier/services/certmgr/controller"

	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/vizier/services/shared/log/logwriter"
)

func main() {
	pflag.String("namespace", "pl", "The namespace of Vizier")

	services.SetupService("certmgr-service", 50900)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()

	err := logwriter.SetupLogger(viper.GetString("cloud_connector_addr"), viper.GetString("pod_name"), "certmgr-service")
	if err != nil {
		log.WithError(err).Fatal("Could not connect to cloud connector for log forwarding")
	}

	log.WithField("service", "certmgr-service").Info("Starting service")

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	k8sAPI, err := controller.NewK8sAPI(viper.GetString("namespace"))
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to K8S API")
	}

	env := certmgrenv.New()
	server := controller.NewServer(env, k8sAPI)

	s := services.NewPLServer(env, mux)
	certmgrpb.RegisterCertMgrServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
