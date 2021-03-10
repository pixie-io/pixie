package cmd

import (
	"os"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/live"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/script"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/vizier"
)

func init() {
	LiveCmd.Flags().StringP("bundle", "b", "", "Path/URL to bundle file")
	LiveCmd.Flags().StringP("file", "f", "", "Script file, specify - for STDIN")
	LiveCmd.Flags().BoolP("new_autocomplete", "n", false, "Whether to use the new autocomplete")

	LiveCmd.Flags().BoolP("all-clusters", "d", false, "Run script across all clusters")
	LiveCmd.Flags().StringP("cluster", "c", "", "Run only on selected cluster")
	LiveCmd.Flags().MarkHidden("all-clusters")

}

// LiveCmd is the "query" command.
var LiveCmd = &cobra.Command{
	Use:   "live",
	Short: "Interactive Pixie Views",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")

		useNewAC, _ := cmd.Flags().GetBool("new_autocomplete")

		br := mustCreateBundleReader()
		var execScript *script.ExecutableScript
		var err error
		scriptFile, _ := cmd.Flags().GetString("file")
		var scriptArgs []string

		if scriptFile == "" {
			if len(args) == 0 {
				utils.Error("Expected script_name with script args.")
				os.Exit(1)
			}
			scriptName := args[0]
			execScript = br.MustGetScript(scriptName)
			scriptArgs = args[1:]
		} else {
			execScript, err = loadScriptFromFile(scriptFile)
			if err != nil {
				utils.WithError(err).Error("Failed to get query string")
				os.Exit(1)
			}
			scriptArgs = args
		}

		fs := execScript.GetFlagSet()
		if fs != nil {
			if err := fs.Parse(scriptArgs); err != nil {
				utils.WithError(err).Error("Failed to parse script flags")
				os.Exit(1)
			}
			err := execScript.UpdateFlags(fs)
			if err != nil {
				utils.WithError(err).Error("Error parsing script flags")
				os.Exit(1)
			}
		}

		cloudConn, err := utils.GetCloudClientConnection(cloudAddr)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Could not connect to cloud")
		}
		aClient := cloudapipb.NewAutocompleteServiceClient(cloudConn)
		allClusters, _ := cmd.Flags().GetBool("all-clusters")
		selectedCluster, _ := cmd.Flags().GetString("cluster")
		clusterUUID := uuid.FromStringOrNil(selectedCluster)
		if !allClusters && clusterUUID == uuid.Nil {
			clusterUUID, err = vizier.GetCurrentOrFirstHealthyVizier(cloudAddr)
			if err != nil {
				utils.WithError(err).Error("Could not fetch healthy vizier")
				os.Exit(1)
			}
		}

		viziers := vizier.MustConnectDefaultVizier(cloudAddr, allClusters, clusterUUID)
		lv, err := live.New(br, viziers, cloudAddr, aClient, execScript, useNewAC, clusterUUID)
		if err != nil {
			utils.WithError(err).Error("Failed to initialize live view")
			os.Exit(1)
		}

		if err := lv.Run(); err != nil {
			utils.WithError(err).Error("Failed to run live view")
			os.Exit(1)
		}
	},
}
