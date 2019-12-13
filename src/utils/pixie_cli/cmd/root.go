package cmd

import (
	"os"

	"github.com/fatih/color"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/update"
)

func init() {
	// Flags that are relevant to all sub-commands.
	RootCmd.PersistentFlags().StringP("cloud_addr", "a", "withpixie.ai:443", "The address of Pixie Cloud")
	viper.BindPFlag("cloud_addr", RootCmd.PersistentFlags().Lookup("cloud_addr"))

	RootCmd.PersistentFlags().BoolP("y", "y", false, "Whether to accept all user input")
	viper.BindPFlag("y", RootCmd.PersistentFlags().Lookup("y"))

	RootCmd.AddCommand(VersionCmd)
	RootCmd.AddCommand(AuthCmd)
	RootCmd.AddCommand(CollectLogsCmd)
	RootCmd.AddCommand(InstallCertsCmd)
	RootCmd.AddCommand(DeployCmd)
	RootCmd.AddCommand(DeleteCmd)
	RootCmd.AddCommand(LoadClusterSecretsCmd)
	RootCmd.AddCommand(UpdateCmd)
	RootCmd.AddCommand(ProxyCmd)
	RootCmd.AddCommand(QueryCmd)
	RootCmd.AddCommand(GetCmd)

	// Super secret flags for Pixies.
	RootCmd.PersistentFlags().MarkHidden("cloud_addr")
}

// RootCmd is the base command for Cobra.
var RootCmd = &cobra.Command{
	Use:   "pixie-admin",
	Short: "Pixie admin cli",
	// TODO(zasgar): Add description and update this.
	Long: `Pixie Description (TBD)`,
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		p := cmd
		for p != nil && p != UpdateCmd {
			p = p.Parent()
		}
		if p == UpdateCmd {
			return
		}
		versionStr := update.UpdatesAvailable(viper.GetString("cloud_addr"))
		if versionStr != "" {
			c := color.New(color.Bold, color.FgGreen)
			_, _ = c.Fprintf(os.Stderr, "Update to version \"%s\" available. Run \"pixie update cli\" to update.\n", versionStr)
		}
	},
}

// Execute is the main function for the Cobra CLI.
func Execute() {
	// Must call after all flags are setup.
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	if err := RootCmd.Execute(); err != nil {
		log.WithError(err).Fatal("Error executing command")
	}
}
