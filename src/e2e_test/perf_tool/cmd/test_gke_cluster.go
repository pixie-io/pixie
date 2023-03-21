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

package cmd

import (
	"context"
	"os"
	"sync"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster/gke"
	"px.dev/pixie/src/pixie_cli/pkg/components"
)

// TestGKEClusterCmd tests `gke.ClusterProvider` by creating a cluster.
var TestGKEClusterCmd = &cobra.Command{
	Use:   "test_gke_cluster",
	Short: "Test creation of GKE cluster",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlags(cmd.Flags())
	},
	Run: func(cmd *cobra.Command, args []string) {
		os.Exit(testGKEClusterCmd(cmd))
	},
}

func init() {
	TestGKEClusterCmd.Flags().Int("num_clusters", 1, "Number of GKE clusters to create in parallel")

	TestGKEClusterCmd.Flags().String("gke_project", "pl-pixies", "The gcloud project to use for GKE clusters")
	TestGKEClusterCmd.Flags().String("gke_zone", "us-west1-a", "The gcloud zone to use for GKE clusters")
	TestGKEClusterCmd.Flags().String("gke_network", "dev", "The gcloud network to use for GKE clusters")
	TestGKEClusterCmd.Flags().String("gke_subnet", "", "The subnetwork to use for GKE clusters, if empty a subnetwork will be created on demand")
	TestGKEClusterCmd.Flags().String("gke_security_group", "gke-security-groups@pixielabs.ai", "The security group to use for GKE clusters")
	RootCmd.AddCommand(TestGKEClusterCmd)
}

func testGKEClusterCmd(*cobra.Command) int {
	numClustersToGet := viper.GetInt("num_clusters")

	provider, err := gke.NewClusterProvider(&gke.ClusterOptions{
		Project:       viper.GetString("gke_project"),
		Zone:          viper.GetString("gke_zone"),
		Network:       viper.GetString("gke_network"),
		Subnet:        viper.GetString("gke_subnet"),
		SecurityGroup: viper.GetString("gke_security_group"),
	})
	if err != nil {
		log.WithError(err).Fatal("failed to create GKE cluster provider")
	}
	defer provider.Close()

	cleanupCh := make(chan bool)
	clustersCreatedWG := sync.WaitGroup{}
	doneWG := sync.WaitGroup{}
	for i := 0; i < numClustersToGet; i++ {
		clustersCreatedWG.Add(1)
		doneWG.Add(1)
		go func() {
			defer doneWG.Done()
			numNodes := int32(1)
			machineType := "n2-standard-4"
			log.Infof("Requesting cluster with %d %s nodes", numNodes, machineType)
			clusterCtx, cleanup, err := provider.GetCluster(context.Background(), &experimentpb.ClusterSpec{
				NumNodes: numNodes,
				Node: &experimentpb.NodeSpec{
					MachineType: machineType,
				},
			})
			if err != nil {
				log.WithError(err).Fatal("failed to create cluster")
			}
			defer clusterCtx.Close()
			defer cleanup()
			clustersCreatedWG.Done()
			<-cleanupCh
		}()
	}

	clustersCreatedWG.Wait()
	for !components.YNPrompt("ready to cleanup clusters?", true) {
	}
	close(cleanupCh)
	doneWG.Wait()
	return 0
}
