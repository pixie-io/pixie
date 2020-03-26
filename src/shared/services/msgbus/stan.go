package msgbus

import (
	"github.com/nats-io/nats.go"
	"github.com/nats-io/stan.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

func init() {
	pflag.String("stan_cluster_id", "pl-stan", "The cluster ID of the stan cluster.")
}

// MustConnectSTAN tries to connect to the STAN message bus.
func MustConnectSTAN(nc *nats.Conn, clientID string) stan.Conn {
	stanClusterID := viper.GetString("stan_cluster_id")

	sc, err := stan.Connect(stanClusterID, clientID, stan.NatsConn(nc))
	if err != nil {
		log.WithError(err).WithField("ClusterID", stanClusterID).Fatal("Failed to connect to STAN")
	}

	log.WithField("ClusterID", stanClusterID).Info("Connected to STAN")

	return sc
}
