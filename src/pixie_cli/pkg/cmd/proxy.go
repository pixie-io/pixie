package cmd

import (
	"os"
	"os/signal"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

func init() {
	ProxyCmd.Flags().StringP("namespace", "n", "pl", "The namespace to install K8s secrets to")
	viper.BindPFlag("namespace", ProxyCmd.Flags().Lookup("namespace"))
}

// ProxyCmd is the "proxy" command.
var ProxyCmd = &cobra.Command{
	Use:   "proxy",
	Short: "Create a proxy connection to Pixie's vizier",
	Run: func(cmd *cobra.Command, args []string) {
		ns, _ := cmd.Flags().GetString("namespace")
		p := k8s.NewVizierProxy(ns)
		if err := p.Run(); err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to start proxy")
		}

		stop := make(chan os.Signal, 1)
		signal.Notify(stop, os.Interrupt)

		// Wait for interrupt.
		<-stop
		utils.Info("Stopping proxy")
		_ = p.Stop()
	},
}
