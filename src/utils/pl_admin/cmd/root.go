package cmd

import (
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

func init() {
	// Flags for install-certs.
	pflag.String("cert_path", "", "Directory to save certs in")
	pflag.String("ca_cert", "", "Path to CA cert (optional)")
	pflag.String("ca_key", "", "Path to CA key (optional)")
	// --namespace is also a flag for deploy.
	pflag.String("namespace", "pl", "The namespace to install certs or Pixie K8s secrets to")

	// Flags for deploy.
	pflag.String("extract_yaml", "", "Directory to extract the Pixie yamls to")
	pflag.String("use_version", "", "Pixie version to deploy")
	pflag.Bool("check", false, "Check whether the cluster can run Pixie")
	pflag.String("registration_key", "", "The registration key to use for this cluster")
	pflag.String("credentials_file", "", "Location of the Pixie credentials file")
	pflag.String("secret_name", "pl-image-secret", "The name of the secret used to access the Pixie images")

	// Flags for delete.
	pflag.Bool("clobber_namespace", false, "Whether to delete all dependencies in the cluster")

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
	installCertsCmd.Flags().StringP("namespace", "n", "pl", "The namespace to install certs to")
	viper.BindPFlag("namespace", installCertsCmd.Flags().Lookup("namespace"))

	deployCmd := NewCmdDeploy()
	RootCmd.AddCommand(deployCmd)
	deployCmd.Flags().StringP("extract_yaml", "e", "", "Directory to extract the Pixie yamls to")
	viper.BindPFlag("extract_yaml", deployCmd.Flags().Lookup("extract_yaml"))
	deployCmd.Flags().StringP("use_version", "v", "", "Pixie version to deploy")
	viper.BindPFlag("use_version", deployCmd.Flags().Lookup("use_version"))
	deployCmd.Flags().BoolP("check", "c", false, "Check whether the cluster can run Pixie")
	viper.BindPFlag("check", deployCmd.Flags().Lookup("check"))
	deployCmd.Flags().StringP("registration_key", "k", "", "The registration key to use for this cluster")
	viper.BindPFlag("registration_key", deployCmd.Flags().Lookup("registration_key"))
	deployCmd.Flags().StringP("credentials_file", "f", "", "Location of the Pixie credentials file")
	viper.BindPFlag("credentials_file", deployCmd.Flags().Lookup("credentials_file"))
	deployCmd.Flags().StringP("secret_name", "s", "pl-image-secret", "The name of the secret used to access the Pixie images")
	viper.BindPFlag("credentials_file", deployCmd.Flags().Lookup("credentials_file"))
	deployCmd.Flags().StringP("namespace", "n", "pl", "The namespace to install K8s secrets to")
	viper.BindPFlag("namespace", deployCmd.Flags().Lookup("namespace"))

	deleteCmd := NewCmdDeletePixie()
	RootCmd.AddCommand(deleteCmd)
	deleteCmd.Flags().BoolP("clobber_namespace", "d", false, "Whether to delete all dependencies in the cluster")
	viper.BindPFlag("clobber_namespace", deleteCmd.Flags().Lookup("clobber_namespace"))
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
