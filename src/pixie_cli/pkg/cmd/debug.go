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
	"strings"
	"time"

	"github.com/fatih/color"
	"github.com/gofrs/uuid"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
)

func init() {
	DebugCmd.AddCommand(DebugLogCmd)
	DebugCmd.AddCommand(DebugPodsCmd)
	DebugCmd.AddCommand(DebugContainersCmd)
	DebugCmd.PersistentFlags().StringP("cluster", "c", "", "Run only on selected cluster")

	DebugLogCmd.Flags().BoolP("previous", "p", false, "Show log from previous pod instead.")
	DebugLogCmd.Flags().StringP("container", "n", "", "The container to get logs from.")

	DebugPodsCmd.Flags().StringP("plane", "p", "all", "Optional filter for the plane (data, control, all)")
	DebugContainersCmd.Flags().StringP("plane", "p", "all", "Optional filter for the plane (data, control, all)")
}

// DebugCmd has internal debug functionality.
var DebugCmd = &cobra.Command{
	Use:    "debug",
	Short:  "Debug commands used by Pixie",
	Hidden: true,
}

// Gets the current Vizier if there is one in kubeconfig, even if it isn't healthy.
// Gets the first healthy Vizier otherwise. This is intentionally different from other px commands,
// since debug commands may run on an unhealthy Vizier.
func getVizier(cloudAddr string) (uuid.UUID, error) {
	id, err := vizier.GetCurrentVizier(cloudAddr)
	if err != nil || id == uuid.Nil {
		id, err = vizier.FirstHealthyVizier(cloudAddr)
	}
	return id, err
}

// DebugLogCmd is the log debug command.
var DebugLogCmd = &cobra.Command{
	Use:   "log",
	Short: "Show log for vizier pods",
	Run: func(cmd *cobra.Command, args []string) {
		if len(args) != 1 {
			utils.Fatal("Must supply a single argument pod name")
		}

		podName := args[0]
		var err error

		cloudAddr := viper.GetString("cloud_addr")
		selectedCluster, _ := cmd.Flags().GetString("cluster")
		clusterID := uuid.FromStringOrNil(selectedCluster)
		container, _ := cmd.Flags().GetString("container")

		if clusterID == uuid.Nil {
			clusterID, err = getVizier(cloudAddr)
			if err != nil {
				utils.WithError(err).Fatal("Could not fetch healthy vizier")
			}
		}

		fmt.Printf("Pod Name: %s\n", podName)
		fmt.Printf("Cluster ID : %s\n", clusterID.String())
		fmt.Printf("Container: %s\n", container)

		conn, err := vizier.ConnectionToVizierByID(cloudAddr, clusterID)
		if err != nil {
			utils.WithError(err).Fatal("Could not connect to vizier")
		}

		prev, _ := cmd.Flags().GetBool("previous")
		ctx, cleanup := utils.WithSignalCancellable(context.Background())
		defer cleanup()
		resp, err := conn.DebugLogRequest(ctx, podName, prev, container)
		if err != nil {
			utils.WithError(err).Fatal("Logging failed")
		}

		for v := range resp {
			if v != nil {
				if v.Err != nil {
					utils.WithError(v.Err).Fatal("Failed to get logs")
				} else {
					fmt.Printf("%s", strings.ReplaceAll(v.Data, "\n",
						fmt.Sprintf("\n%s ", color.GreenString("[%s]", podName))))
				}
			}
		}
	},
}

func fetchVizierPods(cloudAddr, selectedCluster, plane string) ([]*vizierpb.VizierPodStatus, error) {
	clusterID := uuid.FromStringOrNil(selectedCluster)
	showCtrl := plane == "all" || plane == "control"
	showData := plane == "all" || plane == "data"

	if clusterID == uuid.Nil {
		var err error
		clusterID, err = getVizier(cloudAddr)
		if err != nil {
			return nil, err
		}
	}

	fmt.Printf("Cluster ID : %s\n", clusterID.String())

	conn, err := vizier.ConnectionToVizierByID(cloudAddr, clusterID)
	if err != nil {
		return nil, err
	}

	ctx, cleanup := utils.WithSignalCancellable(context.Background())
	defer cleanup()
	resp, err := conn.DebugPodsRequest(ctx)
	if err != nil {
		return nil, err
	}

	var results []*vizierpb.VizierPodStatus

	for v := range resp {
		if v != nil {
			if v.Err != nil {
				return nil, v.Err
			}
			if showCtrl {
				results = append(results, v.ControlPlanePods...)
			}
			if showData {
				results = append(results, v.DataPlanePods...)
			}
		}
	}
	return results, nil
}

// DebugPodsCmd is the pods debug command.
var DebugPodsCmd = &cobra.Command{
	Use:   "pods",
	Short: "List vizier pods",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		selectedCluster, _ := cmd.Flags().GetString("cluster")
		plane, _ := cmd.Flags().GetString("plane")
		pods, err := fetchVizierPods(cloudAddr, selectedCluster, plane)
		if err != nil {
			utils.WithError(err).Fatal("Could not fetch Vizier pods")
		}
		w := components.CreateStreamWriter("table", os.Stdout)
		defer w.Finish()
		w.SetHeader("pods", []string{"Name", "Phase", "Restarts", "Message", "Reason", "Start Time"})
		for _, pod := range pods {
			_ = w.Write([]interface{}{
				pod.Name, pod.Phase, pod.RestartCount, pod.Message, pod.Reason, time.Unix(0, pod.CreatedAt),
			})
		}
	},
}

// DebugContainersCmd is the containers debug command.
var DebugContainersCmd = &cobra.Command{
	Use:   "containers",
	Short: "List vizier containers",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		selectedCluster, _ := cmd.Flags().GetString("cluster")
		plane, _ := cmd.Flags().GetString("plane")
		pods, err := fetchVizierPods(cloudAddr, selectedCluster, plane)
		if err != nil {
			utils.WithError(err).Fatal("Could not fetch Vizier pods")
		}
		w := components.CreateStreamWriter("table", os.Stdout)
		defer w.Finish()
		w.SetHeader("containers", []string{"Name", "Pod", "State", "Restarts", "Message", "Reason", "Start Time"})
		for _, pod := range pods {
			for _, c := range pod.ContainerStatuses {
				_ = w.Write([]interface{}{
					c.Name, pod.Name, c.ContainerState, c.RestartCount, c.Message, c.Reason, time.Unix(0, c.StartTimestampNS),
				})
			}
		}
	},
}
