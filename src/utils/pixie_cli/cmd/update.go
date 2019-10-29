package cmd

import (
	"fmt"
	"os"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/shared/version"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/update"
)

func init() {
	UpdateCmd.AddCommand(CLIUpdateCmd)
}

// UpdateCmd is the "update" sub-command of the CLI.
var UpdateCmd = &cobra.Command{
	Use:   "update",
	Short: "Update Pixie/CLI",
	Run: func(cmd *cobra.Command, args []string) {
		log.Info("Nothing here... Please execute one of the subcommands")
		cmd.Help()
		return
	},
}

// CLIUpdateCmd is the cli subcommand of the "update" command.
var CLIUpdateCmd = &cobra.Command{
	Use:   "cli",
	Short: "Run updates of CLI/Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		currentSemver := version.GetVersion().Semver()
		updater := update.NewCLIUpdater(viper.GetString("cloud_addr"))
		versions, err := updater.GetAvailableVersions(currentSemver)
		if err != nil {
			panic(err)
		}

		if len(versions) <= 0 {
			fmt.Println("No updates available")
			return
		}

		if ok, err := updater.IsUpdatable(); !ok || err != nil {
			if err != nil {
				panic(err)
			}
			fmt.Println("cannot perform update, it's likely the file is not in a writable path.")
			os.Exit(1)
			// TODO(zasgar): Provide a means to update this as well.
		}

		selectedVersion := versions[0]
		fmt.Printf("Updating to version: %s\n", selectedVersion)
		err = updater.UpdateSelf(selectedVersion)
		if err != nil {
			fmt.Println("Failed to apply update")
			panic(err)
		}
		fmt.Println("Update completed successfully")
	},
}
