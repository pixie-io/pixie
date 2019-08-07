package main

import (
	"crypto/tls"
	"net/http"
	"time"

	"github.com/coreos/etcd/clientv3"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"go.etcd.io/etcd/pkg/transport"
	"pixielabs.ai/pixielabs/src/services/common"
	"pixielabs.ai/pixielabs/src/services/common/healthz"
	"pixielabs.ai/pixielabs/src/services/common/httpmiddleware"
	"pixielabs.ai/pixielabs/src/shared/version"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadataenv"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
)

func etcdTLSConfig() (*tls.Config, error) {
	tlsCert := viper.GetString("client_tls_cert")
	tlsKey := viper.GetString("client_tls_key")
	tlsCACert := viper.GetString("tls_ca_cert")

	tlsInfo := transport.TLSInfo{
		CertFile:      tlsCert,
		KeyFile:       tlsKey,
		TrustedCAFile: tlsCACert,
	}

	return tlsInfo.ClientConfig()
}

func main() {
	log.WithField("service", "metadata").Info("Starting service")

	pflag.String("md_etcd_server", "https://pl-etcd-client.pl.svc.cluster.local:2379", "The address to metadata etcd server.")
	common.SetupService("metadata", 50400)
	common.SetupSSLClientFlags()
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.CheckSSLClientFlags()
	common.SetupServiceLogging()

	var tlsConfig *tls.Config
	if !viper.GetBool("disable_ssl") {
		var err error
		tlsConfig, err = etcdTLSConfig()
		if err != nil {
			log.WithError(err).Fatal("Failed to load SSL for ETCD")
		}
	}

	etcdClient, err := clientv3.New(clientv3.Config{
		Endpoints:   []string{viper.GetString("md_etcd_server")},
		DialTimeout: 5 * time.Second,
		TLS:         tlsConfig,
	})
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to etcd.")
	}
	defer etcdClient.Close()

	etcdMds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		log.WithError(err).Fatal("Failed to create etcd metadata store")
	}
	defer etcdMds.Close()

	// TODO(michelle): Add code for leader election. For now, since we only have one metadata
	// service, it is always the leader.
	agtMgr := controllers.NewAgentManager(etcdClient, etcdMds, true)
	keepAlive := true
	go func() {
		for keepAlive {
			agtMgr.UpdateAgentState()
			time.Sleep(10 * time.Second)
		}
	}()

	mc, err := controllers.NewMessageBusController("pl-nats", "update_agent", agtMgr)

	if err != nil {
		log.WithError(err).Fatal("Failed to connect to message bus")
	}
	defer mc.Close()

	// Listen for K8s metadata updates.
	mdHandler, err := controllers.NewMetadataHandler(etcdMds)
	if err != nil {
		log.WithError(err).Fatal("Failed to create metadata handler")
	}

	mdHandler.ProcessAgentUpdates()

	_, err = controllers.NewK8sMetadataController(mdHandler)

	env, err := metadataenv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}
	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	server, err := controllers.NewServer(env, agtMgr)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs")
	}

	log.Info("Metadata Server: " + version.GetVersion().ToString())

	s := common.NewPLServer(env,
		httpmiddleware.WithBearerAuthMiddleware(env, mux))
	metadatapb.RegisterMetadataServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
