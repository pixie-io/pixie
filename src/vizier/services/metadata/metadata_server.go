package main

import (
	"net/http"
	"time"

	"github.com/coreos/etcd/clientv3"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/services/common"
	"pixielabs.ai/pixielabs/src/services/common/healthz"
	"pixielabs.ai/pixielabs/src/services/common/httpmiddleware"
	"pixielabs.ai/pixielabs/src/shared/version"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadataenv"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
)

func main() {
	log.WithField("service", "metadata").Info("Starting service")

	common.SetupService("metadata", 50400)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	etcdClient, err := clientv3.New(clientv3.Config{
		Endpoints:   []string{"http://pl-etcd-client.pl.svc.cluster.local:2379"},
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to etcd.")
	}
	defer etcdClient.Close()

	etcdMds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		log.WithError(err).Fatal("Failed to create etcd metadata store")
	}

	// TODO(michelle): Add code for leader election. For now, since we only have one metadata
	// service, it is always the leader.
	agtMgr := controllers.NewAgentManager(etcdClient, true)
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

	mdCh := mdHandler.GetChannel()

	_, err = controllers.NewK8sMetadataController(mdCh)

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
		httpmiddleware.WithNewSessionMiddleware(
			httpmiddleware.WithBearerAuthMiddleware(env, mux)))
	metadatapb.RegisterMetadataServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
