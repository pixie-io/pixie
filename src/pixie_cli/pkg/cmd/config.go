package cmd

import (
	"os"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"github.com/gogo/protobuf/types"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	cliLog "pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/vizier"
	"pixielabs.ai/pixielabs/src/utils"
)

// ConfigCmd is the command for geting/updating the cluster config.
var ConfigCmd = &cobra.Command{
	Use:   "config",
	Short: "Get/update the current cluster config",
	Run: func(cmd *cobra.Command, args []string) {
		// Check cluster ID.
		clusterID, _ := cmd.Flags().GetString("cluster_id")
		if clusterID == "" {
			cliLog.Error("Need to specify cluster ID in flags: --cluster_id=<cluster-id>")
			return
		}
		clusterUUID, err := uuid.FromString(clusterID)
		if err != nil {
			cliLog.Errorf("Invalid cluster ID: %s\n", err.Error())
			return
		}
		clusterIDPb := utils.ProtoFromUUID(clusterUUID)

		cloudAddr := viper.GetString("cloud_addr")
		l, err := vizier.NewLister(cloudAddr)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to create Vizier lister")
		}

		vzInfo, err := l.GetVizierInfo(clusterUUID)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to get Vizier info")
		}

		if len(vzInfo) == 0 {
			cliLog.Errorf("Invalid cluster ID: %s", clusterID)
			os.Exit(1)
		}

		update, _ := cmd.Flags().GetBool("update")
		if !update {
			// Update not specified. User just wants to get the config.
			cliLog.Infof("%s: %t", "PassthroughEnabled", vzInfo[0].Config.PassthroughEnabled)
			return
		}

		ptEnabled, _ := cmd.Flags().GetString("passthrough")
		if ptEnabled == "" {
			return // No config setting specified for passthrough.
		}

		if ptEnabled != "true" && ptEnabled != "false" {
			cliLog.Infof("Invalid option specified for passthrough: %s. Expected (true|false)", ptEnabled)
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
			cliLog.Errorf("Error updating config: %s", err.Error())
		}
		cliLog.Info("Successfully updated config")
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
