package cmd

import (
	"context"
	"fmt"
	"os"
	"strings"

	"github.com/fatih/color"
	"github.com/gofrs/uuid"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	cliLog "pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/vizier"
)

func init() {
	DebugCmd.AddCommand(DebugLogCmd)
	DebugCmd.PersistentFlags().StringP("cluster", "c", "", "Run only on selected cluster")

	DebugLogCmd.Flags().BoolP("previous", "p", false, "Show log from previous pod instead.")
	DebugLogCmd.Flags().StringP("container", "n", "", "The container to get logs from.")
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
