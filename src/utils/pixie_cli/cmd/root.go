package cmd

import (
	"os"

	"github.com/fatih/color"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	analytics "gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/update"
)

func init() {
	// Flags that are relevant to all sub-commands.
	RootCmd.PersistentFlags().StringP("cloud_addr", "a", "withpixie.ai:443", "The address of Pixie Cloud")
	viper.BindPFlag("cloud_addr", RootCmd.PersistentFlags().Lookup("cloud_addr"))

	RootCmd.PersistentFlags().BoolP("y", "y", false, "Whether to accept all user input")
	viper.BindPFlag("y", RootCmd.PersistentFlags().Lookup("y"))

	RootCmd.PersistentFlags().BoolP("quiet", "q", false, "quiet mode")
	viper.BindPFlag("quiet", RootCmd.PersistentFlags().Lookup("quiet"))

	RootCmd.AddCommand(VersionCmd)
	RootCmd.AddCommand(AuthCmd)
	RootCmd.AddCommand(CollectLogsCmd)
	RootCmd.AddCommand(InstallCertsCmd)
	RootCmd.AddCommand(DeployCmd)
	RootCmd.AddCommand(DeleteCmd)
	RootCmd.AddCommand(LoadClusterSecretsCmd)
	RootCmd.AddCommand(UpdateCmd)
	RootCmd.AddCommand(ProxyCmd)
	RootCmd.AddCommand(RunCmd)
	RootCmd.AddCommand(GetCmd)

	RootCmd.AddCommand(CreateBundle)

	// Super secret flags for Pixies.
	RootCmd.PersistentFlags().MarkHidden("cloud_addr")
}

func printPixie() {
	pixie := `
  ___  _       _
 | _ \(_)__ __(_) ___
 |  _/| |\ \ /| |/ -_)
 |_|  |_|/_\_\|_|\___|
`
	c := color.New(color.FgHiGreen)
	c.Fprintln(os.Stderr, pixie)
}

// RootCmd is the base command for Cobra.
var RootCmd = &cobra.Command{
	Use:   "px",
	Short: "Pixie CLI",
	// TODO(zasgar): Add description and update this.
	Long: `The Pixie command line interface.`,
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		quiet, _ := cmd.Flags().GetBool("quiet")
		if !quiet {
			printPixie()
		}

		p := cmd

		if p != nil {
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Exec CMD",
				Properties: analytics.NewProperties().
					Set("cmd", p.Name()),
			})
		}

		for p != nil && p != UpdateCmd {
			p = p.Parent()
		}

		if p == UpdateCmd {
			return
		}
		versionStr := update.UpdatesAvailable(viper.GetString("cloud_addr"))
		if versionStr != "" {
			cmdName := "<NONE>"
			if p != nil {
				cmdName = p.Name()
			}

			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Update Available",
				Properties: analytics.NewProperties().
					Set("cmd", cmdName),
			})
			c := color.New(color.Bold, color.FgGreen)
			_, _ = c.Fprintf(os.Stderr, "Update to version \"%s\" available. Run \"px update cli\" to update.\n", versionStr)
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
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Exec Error",
		})
		log.WithError(err).Fatal("Error executing command")
	}
}
