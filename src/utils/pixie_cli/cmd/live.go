package cmd

import (
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/live"
)

func init() {
	LiveCmd.Flags().StringP("bundle", "b", "", "Path/URL to bundle file")
}

// LiveCmd is the "query" command.
var LiveCmd = &cobra.Command{
	Use:   "live",
	Short: "Interactive Pixie Views",
	Run: func(cmd *cobra.Command, args []string) {
		if len(args) != 1 {
			log.Fatal("Expected a single arg, script_name")
		}
		cloudAddr := viper.GetString("cloud_addr")

		br := mustCreateBundleReader()
		scriptName := args[0]
		execScript := br.MustGetScript(scriptName)

		v := mustConnectDefaultVizier(cloudAddr)

		lv, err := live.New(br, v, execScript)
		if err != nil {
			log.WithError(err).Fatal("Failed to initialize live view")
		}

		if err := lv.Run(); err != nil {
			log.WithError(err).Fatal("Failed to run live view")
		}
	},
}
