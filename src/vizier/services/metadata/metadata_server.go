package main

import (
	"context"
	"crypto/tls"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/clientv3/concurrency"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"go.etcd.io/etcd/pkg/transport"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"
	"pixielabs.ai/pixielabs/src/shared/version"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadataenv"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
)

const (
	cacheFlushPeriod = 1 * time.Second
	cacheClearPeriod = 2 * time.Second
)

func init() {
	pflag.String("md_etcd_server", "https://pl-etcd-client.pl.svc:2379", "The address to metadata etcd server.")
	pflag.String("cluster_id", "", "The Cluster ID to use for Pixie Cloud")
}

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
	log.WithField("service", "metadata").
		WithField("version", version.GetVersion().ToString()).
		Info("Starting service")

	services.SetupService("metadata", 50400)
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.CheckSSLClientFlags()
	services.SetupServiceLogging()

	flush := services.InitDefaultSentry(viper.GetString("cluster_id"))
	defer flush()

	var tlsConfig *tls.Config
	if !viper.GetBool("disable_ssl") {
		var err error
		tlsConfig, err = etcdTLSConfig()
		if err != nil {
			log.WithError(err).Fatal("Failed to load SSL for ETCD")
		}
	}

	// Connect to etcd.
	etcdClient, err := clientv3.New(clientv3.Config{
		Endpoints:   []string{viper.GetString("md_etcd_server")},
		DialTimeout: 5 * time.Second,
		TLS:         tlsConfig,
	})
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to etcd at " + viper.GetString("md_etcd_server"))
	}
	defer etcdClient.Close()

	// Set up leader election.
	leaseResp, err := etcdClient.Grant(context.TODO(), int64(10))
	if err != nil {
		log.Fatal("Could not get grant for leader election session")
	}
	leaseID := leaseResp.ID
	session, err := concurrency.NewSession(etcdClient, concurrency.WithLease(leaseID))
	if err != nil {
		log.WithError(err).Fatal("Could not create new session for etcd")
	}
	defer session.Close()

	leaderElection := etcd.NewLeaderElection(session)
	isLeader := false
	// MDS should block on its first attempt to become leader, so that it won't
	// skip syncing if it is supposed to be the leader.
	err = leaderElection.Campaign()
	if err == nil {
		isLeader = true
	}
	go leaderElection.RunElection(&isLeader)
	go func() {
		ch := make(chan os.Signal)
		signal.Notify(ch, syscall.SIGINT, syscall.SIGTERM)
		<-ch
		leaderElection.Stop()
	}()

	etcdDS := kvstore.NewEtcdStore(etcdClient)
	cache := kvstore.NewCache(etcdDS)
	mds, err := controllers.NewKVMetadataStore(cache)
	if err != nil {
		log.WithError(err).Fatal("Could not create metadata store")
	}

	agtMgr := controllers.NewAgentManager(mds)
	keepAlive := true
	go func() {
		for keepAlive {
			if isLeader {
				agtMgr.UpdateAgentState()
			}
			time.Sleep(10 * time.Second)
		}
	}()
	go func() {
		for keepAlive {
			if isLeader {
				cache.FlushToDatastore()
				time.Sleep(cacheFlushPeriod)
			} else {
				cache.Clear()
				time.Sleep(cacheClearPeriod)
			}

		}
	}()

	defer func() {
		keepAlive = false
	}()

	// Listen for K8s metadata updates.
	mdHandler, err := controllers.NewMetadataHandler(mds, &isLeader)
	if err != nil {
		log.WithError(err).Fatal("Failed to create metadata handler")
	}
	mdHandler.AddSubscriber(agtMgr)

	mdHandler.ProcessSubscriberUpdates()

	mc, err := controllers.NewMessageBusController("pl-nats", "update_agent", agtMgr, mds, mdHandler, &isLeader)

	if err != nil {
		log.WithError(err).Fatal("Failed to connect to message bus")
	}
	defer mc.Close()

	k8sMd, err := controllers.NewK8sMetadataController(mdHandler)
	mds.SetClusterCIDR(k8sMd.GetClusterCIDR())

	// Set up server.
	env, err := metadataenv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to create api environment")
	}
	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	server, err := controllers.NewServer(env, agtMgr, mds)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs")
	}

	log.Info("Metadata Server: " + version.GetVersion().ToString())

	s := services.NewPLServer(env,
		httpmiddleware.WithBearerAuthMiddleware(env, mux))
	metadatapb.RegisterMetadataServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
