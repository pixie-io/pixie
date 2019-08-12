package cmd

import (
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

func init() {
	pflag.String("cert_path", "", "Directory to save certs in")
	pflag.String("ca_cert", "", "Path to CA cert (optional)")
	pflag.String("ca_key", "", "Path to CA key (optional)")

	pflag.Parse()

	// Must call after all flags are setup.
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	RootCmd.AddCommand(NewCmdVersion())

	installCertsCmd := NewCmdInstallCerts()
	RootCmd.AddCommand(installCertsCmd)
	installCertsCmd.Flags().StringP("cert_path", "p", "", "Directory to save certs in")
	viper.BindPFlag("cert_path", installCertsCmd.Flags().Lookup("cert_path"))
	installCertsCmd.Flags().StringP("ca_cert", "c", "", "Path to CA cert (optional)")
	viper.BindPFlag("ca_cert", installCertsCmd.Flags().Lookup("ca_cert"))
	installCertsCmd.Flags().StringP("ca_key", "k", "", "Path to CA key (optional)")
	viper.BindPFlag("ca_key", installCertsCmd.Flags().Lookup("ca_key"))

}

// RootCmd is the base command for Cobra.
var RootCmd = &cobra.Command{
	Use:   "pixie-admin",
	Short: "Pixie admin cli",
	// TODO(zasgar): Add description and update this.
	Long: `Pixie Description (TBD)`,
}

// Execute is the main function for the Cobra CLI.
func Execute() {
	if err := RootCmd.Execute(); err != nil {
		log.WithError(err).Fatal("Error executing command")
	}
}
