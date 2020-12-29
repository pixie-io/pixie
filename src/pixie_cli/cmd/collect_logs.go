package cmd

import (
	"fmt"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	cliLog "pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

func init() {
	CollectLogsCmd.Flags().StringP("namespace", "n", "pl", "The namespace to install K8s secrets to")
	viper.BindPFlag("namespace", CollectLogsCmd.Flags().Lookup("namespace"))
}

// CollectLogsCmd is the "deploy" command.
var CollectLogsCmd = &cobra.Command{
	Use:   "collect-logs",
	Short: "Collect pixie logs on the cluster",
	Run: func(cmd *cobra.Command, args []string) {
		c := k8s.NewLogCollector(viper.GetString("namespace"))
		fName := fmt.Sprintf("pixie_logs_%s.zip", time.Now().Format("20060102150405"))
		err := c.CollectPixieLogs(fName)
		if err != nil {
			log.WithError(err).Fatal("Failed to get log files")
		}

		cliLog.Infof("Logs written to %s", fName)
	},
}
