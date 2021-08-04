/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package msgbus

import (
	"github.com/nats-io/nats.go"
	"github.com/nats-io/stan.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

func init() {
	pflag.String("stan_cluster", "pl-stan", "The cluster ID of the stan cluster.")
}

// MustConnectSTAN tries to connect to the STAN message bus.
func MustConnectSTAN(nc *nats.Conn, clientID string) stan.Conn {
	stanClusterID := viper.GetString("stan_cluster")

	sc, err := stan.Connect(stanClusterID, clientID, stan.NatsConn(nc))
	if err != nil {
		log.WithError(err).WithField("ClusterID", stanClusterID).Fatal("Failed to connect to STAN")
	}

	log.WithField("ClusterID", stanClusterID).Info("Connected to STAN")

	return sc
}
