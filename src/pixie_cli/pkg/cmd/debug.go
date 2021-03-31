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

	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/components"
	cliLog "pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/vizier"
	pl_api_vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
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

// DebugLogCmd is the log debug command.
var DebugLogCmd = &cobra.Command{
	Use:   "log",
	Short: "Show log for vizier pods",
	Run: func(cmd *cobra.Command, args []string) {
		if len(args) != 1 {
			cliLog.Error("Must supply a single argument pod name")
			os.Exit(1)
		}

		podName := args[0]
		var err error

		cloudAddr := viper.GetString("cloud_addr")
		selectedCluster, _ := cmd.Flags().GetString("cluster")
		clusterID := uuid.FromStringOrNil(selectedCluster)
		container, _ := cmd.Flags().GetString("container")

		if clusterID == uuid.Nil {
			clusterID, err = vizier.GetCurrentOrFirstHealthyVizier(cloudAddr)
			if err != nil {
				cliLog.WithError(err).Error("Could not fetch healthy vizier")
				os.Exit(1)
			}
		}

		fmt.Printf("Pod Name: %s\n", podName)
		fmt.Printf("Cluster ID : %s\n", clusterID.String())
		fmt.Printf("Container: %s\n", container)

		conn, err := vizier.ConnectionToVizierByID(cloudAddr, clusterID)
		if err != nil {
			cliLog.WithError(err).Error("Could not connect to vizier")
			os.Exit(1)
		}

		prev, _ := cmd.Flags().GetBool("previous")
		resp, err := conn.DebugLogRequest(context.Background(), podName, prev, container)
		if err != nil {
			cliLog.WithError(err).Error("Logging failed")
			os.Exit(1)
		}

		for v := range resp {
			if v != nil {
				if v.Err != nil {
					cliLog.WithError(v.Err).Error("Failed to get logs")
					os.Exit(1)
				} else {
					fmt.Printf("%s", strings.ReplaceAll(v.Data, "\n",
						fmt.Sprintf("\n%s ", color.GreenString("[%s]", podName))))
				}
			}
		}
	},
}

func fetchVizierPods(cloudAddr, selectedCluster, plane string) ([]*pl_api_vizierpb.VizierPodStatus, error) {
	clusterID := uuid.FromStringOrNil(selectedCluster)
	showCtrl := plane == "all" || plane == "control"
	showData := plane == "all" || plane == "data"

	if clusterID == uuid.Nil {
		var err error
		clusterID, err = vizier.GetCurrentOrFirstHealthyVizier(cloudAddr)
		if err != nil {
			return nil, err
		}
	}

	fmt.Printf("Cluster ID : %s\n", clusterID.String())

	conn, err := vizier.ConnectionToVizierByID(cloudAddr, clusterID)
	if err != nil {
		return nil, err
	}

	resp, err := conn.DebugPodsRequest(context.Background())
	if err != nil {
		return nil, err
	}

	var results []*pl_api_vizierpb.VizierPodStatus

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
			cliLog.WithError(err).Error("Could not fetch Vizier pods")
			os.Exit(1)
		}
		w := components.CreateStreamWriter("table", os.Stdout)
		defer w.Finish()
		w.SetHeader("pods", []string{"Name", "Phase", "Message", "Reason", "Start Time"})
		for _, pod := range pods {
			t := time.Unix(pod.Status.CreatedAt.Seconds, int64(pod.Status.CreatedAt.Nanos))
			_ = w.Write([]interface{}{pod.Name, pod.Status.Phase, pod.Status.Message, pod.Status.Reason, t})
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
			cliLog.WithError(err).Error("Could not fetch Vizier pods")
			os.Exit(1)
		}
		w := components.CreateStreamWriter("table", os.Stdout)
		defer w.Finish()
		w.SetHeader("pods", []string{"Name", "Pod", "State", "Message", "Reason", "Start Time"})
		for _, pod := range pods {
			for _, container := range pod.Status.ContainerStatuses {
				t := time.Unix(0, container.StartTimestampNS)
				_ = w.Write([]interface{}{container.Name, pod.Name, container.ContainerState, container.Message, container.Reason, t})
			}
		}
	},
}
