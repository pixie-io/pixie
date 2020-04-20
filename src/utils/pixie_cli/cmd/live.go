package cmd

import (
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/live"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"
)

func init() {
	LiveCmd.Flags().StringP("bundle", "b", "", "Path/URL to bundle file")
	LiveCmd.Flags().StringP("file", "f", "", "Script file, specify - for STDIN")

}

// LiveCmd is the "query" command.
var LiveCmd = &cobra.Command{
	Use:   "live",
	Short: "Interactive Pixie Views",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")

		br := mustCreateBundleReader()
		var execScript *script.ExecutableScript
		var err error
		scriptFile, _ := cmd.Flags().GetString("file")
		if scriptFile == "" {
			if len(args) > 0 {
				scriptName := args[0]
				execScript = br.MustGetScript(scriptName)
			}
		} else {
			execScript, err = loadScriptFromFile(scriptFile)
			if err != nil {
				log.WithError(err).Fatal("Failed to get query string")
			}
			// Add custom script to bundle reader so we can access it later.
			br.AddScript(execScript)
		}

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
