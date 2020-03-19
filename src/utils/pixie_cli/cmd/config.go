package cmd

import (
	"fmt"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"github.com/gogo/protobuf/types"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/vizier"
)

// ConfigCmd is the command for geting/updating the cluster config.
var ConfigCmd = &cobra.Command{
	Use:   "config",
	Short: "Get/update the current cluster config",
	Run: func(cmd *cobra.Command, args []string) {
		// Check cluster ID.
		clusterID, _ := cmd.Flags().GetString("cluster_id")
		if clusterID == "" {
			fmt.Printf("Need to specify cluster ID in flags: --cluster_id=<cluster-id>\n")
			return
		}
		clusterUUID, err := uuid.FromString(clusterID)
		if err != nil {
			fmt.Printf("Invalid cluster ID: %s\n", err.Error())
			return
		}
		clusterIDPb := utils.ProtoFromUUID(&clusterUUID)

		cloudAddr := viper.GetString("cloud_addr")
		l, err := vizier.NewLister(cloudAddr)
		if err != nil {
			log.WithError(err).Fatal("Failed to create Vizier lister")
		}

		vzInfo, err := l.GetVizierInfo(clusterUUID)
		if err != nil {
			log.WithError(err).Fatal("Failed to get Vizier info")
		}

		if len(vzInfo) == 0 {
			log.WithError(err).Fatal("Invalid cluster ID")
		}

		update, _ := cmd.Flags().GetBool("update")
		if !update {
			// Update not specified. User just wants to get the config.
			fmt.Printf("%s: %t\n", "PassthroughEnabled", vzInfo[0].Config.PassthroughEnabled)
			return
		}

		ptEnabled, _ := cmd.Flags().GetString("passthrough")
		if ptEnabled == "" {
			return // No config setting specified for passthrough.
		}

		if ptEnabled != "true" && ptEnabled != "false" {
			fmt.Printf("Invalid option specified for passthrough: %s. Expected (true|false)\n", ptEnabled)
			return
		}

		req := &cloudapipb.UpdateClusterVizierConfigRequest{
			ID: clusterIDPb,
			ConfigUpdate: &cloudapipb.VizierConfigUpdate{
				PassthroughEnabled: &types.BoolValue{Value: true},
			},
		}

		if ptEnabled == "false" {
			req.ConfigUpdate.PassthroughEnabled.Value = false
		}

		err = l.UpdateVizierConfig(req)
		if err != nil {
			fmt.Printf("Error updating config: %s\n", err.Error())
		}
		fmt.Printf("Successfully updated config\n")
	},
}

func init() {
	ConfigCmd.Flags().BoolP("update", "u", false, "Whether to update the config")
	viper.BindPFlag("update", ConfigCmd.Flags().Lookup("update"))
	ConfigCmd.Flags().StringP("passthrough", "t", "", "Whether pasthrough should be enabled")
	viper.BindPFlag("passthrough", ConfigCmd.Flags().Lookup("passthrough"))
	ConfigCmd.Flags().StringP("cluster_id", "c", "", "The ID of the cluster")
	viper.BindPFlag("cluster_id", ConfigCmd.Flags().Lookup("cluster_id"))
}
