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
	"fmt"
	"os"
	"sort"
	"strings"
	"time"

	"github.com/blang/semver"
	"github.com/dustin/go-humanize"
	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/pixie_cli/pkg/components"
	cliUtils "px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/script"
	"px.dev/pixie/src/utils/shared/k8s"
)

func init() {
	GetCmd.PersistentFlags().StringP("output", "o", "", "Output format: one of: json|proto")

	GetPEMsCmd.Flags().BoolP("all-clusters", "d", false, "Run script across all clusters")
	GetPEMsCmd.Flags().StringP("cluster", "c", "", "Run only on selected cluster")
	GetPEMsCmd.Flags().MarkHidden("all-clusters")

	GetClusterCmd.Flags().Bool("id", false, "Whether to only fetch the cluster ID from the cluster running in the current kubeconfig")
	GetClusterCmd.Flags().Bool("cloud-addr", false, "Whether to only fetch the cloud address from the cluster running in the current kubeconfig")

	GetCmd.AddCommand(GetPEMsCmd)
	GetCmd.AddCommand(GetViziersCmd)
	GetCmd.AddCommand(GetClusterCmd)
}

// GetPEMsCmd is the "get pem" command.
var GetPEMsCmd = &cobra.Command{
	Use:     "pems",
	Aliases: []string{"agents", "pem"},
	Short:   "Get information about running pems",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")
		format = strings.ToLower(format)
		br := mustCreateBundleReader()
		execScript := br.MustGetScript(script.AgentStatusScript)

		allClusters, _ := cmd.Flags().GetBool("all-clusters")
		selectedCluster, _ := cmd.Flags().GetString("cluster")
		clusterID := uuid.FromStringOrNil(selectedCluster)
		var err error
		if !allClusters && clusterID == uuid.Nil {
			clusterID, err = vizier.GetCurrentVizier(cloudAddr)
			if err != nil {
				cliUtils.WithError(err).Fatal("Could not fetch healthy vizier")
			}
		}

		conns := vizier.MustConnectHealthyDefaultVizier(cloudAddr, allClusters, clusterID)

		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()
		if err := vizier.RunScriptAndOutputResults(ctx, conns, execScript, format, false); err != nil {
			cliUtils.Fatalf("Script failed: %s", vizier.FormatErrorMessage(err))
		}
	},
}

// GetViziersCmd is the "get viziers" command.
var GetViziersCmd = &cobra.Command{
	Use:     "viziers",
	Aliases: []string{"clusters"},
	Short:   "Get information about registered viziers",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")
		format = strings.ToLower(format)

		l, err := vizier.NewLister(cloudAddr)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to create Vizier lister")
		}
		vzs, err := l.GetViziersInfo()
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatalln("Failed to get vizier information")
		}

		sort.Slice(vzs, func(i, j int) bool { return vzs[i].ClusterName < vzs[j].ClusterName })

		w := components.CreateStreamWriter(format, os.Stdout)
		defer w.Finish()
		w.SetHeader("viziers", []string{"ClusterName", "ID", "K8s Version", "Operator Version", "Vizier Version", "Last Heartbeat", "Status", "Status Message"})

		for _, vz := range vzs {
			var lastHeartbeat interface{}
			lastHeartbeat = vz.LastHeartbeatNs
			if format == "" || format == "table" {
				if vz.LastHeartbeatNs >= 0 {
					lastHeartbeat = humanize.Time(
						time.Unix(0,
							time.Since(time.Unix(0, vz.LastHeartbeatNs)).Nanoseconds()))
				}
			}
			_ = w.Write([]interface{}{vz.ClusterName, utils.UUIDFromProtoOrNil(vz.ID), vz.ClusterVersion,
				prettyVersion(vz.OperatorVersion), prettyVersion(vz.VizierVersion), lastHeartbeat, vz.Status, vz.StatusMessage})
		}
	},
}

// prettyVersion returns a pretty form of the version string.
func prettyVersion(version string) string {
	sb := strings.Builder{}
	sv, err := semver.Parse(version)
	if err != nil {
		return ""
	}
	sb.WriteString(fmt.Sprintf("%d.%d.%d", sv.Major, sv.Minor, sv.Patch))
	for idx, pre := range sv.Pre {
		if idx == 0 {
			sb.WriteString("-")
		} else {
			sb.WriteString(".")
		}
		sb.WriteString(pre.String())
	}
	return sb.String()
}

// GetClusterCmd is the "get cluster" command to get information about the current kubeconfig cluster.
var GetClusterCmd = &cobra.Command{
	Use:   "cluster",
	Short: "Get information about the current kubeconfig cluster",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("id", cmd.Flags().Lookup("id"))
		viper.BindPFlag("cloud-addr", cmd.Flags().Lookup("cloud-addr"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		id, _ := cmd.Flags().GetBool("id")
		addr, _ := cmd.Flags().GetBool("cloud-addr")

		config := k8s.GetConfig()

		clusterID := vizier.GetClusterIDFromKubeConfig(config)

		if clusterID == uuid.Nil {
			cliUtils.Infof("Unable to find Pixie cluster running in current kubeconfig")
		}

		cloudAddr := vizier.GetCloudAddrFromKubeConfig(config)

		if id {
			fmt.Fprintf(os.Stdout, "%s\n", clusterID)
			return
		}

		if addr {
			fmt.Fprintf(os.Stdout, "%s\n", cloudAddr)
			return
		}

		cliUtils.Infof("Cluster ID: %s\nCloud Address: %s", clusterID, cloudAddr)
	},
}

// GetCmd is the "get" command.
var GetCmd = &cobra.Command{
	Use:   "get",
	Short: "Get information about cluster/edge modules",
}
