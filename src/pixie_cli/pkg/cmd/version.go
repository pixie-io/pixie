package cmd

import (
	"fmt"

	"github.com/spf13/cobra"

	version "px.dev/pixie/src/shared/goversion"
)

// VersionCmd is the "version" command.
var VersionCmd = &cobra.Command{
	Use:   "version",
	Short: "Print the version number of the cli",
	Run: func(cmd *cobra.Command, args []string) {
		fmt.Printf("%s\n", version.GetVersion().ToString())
	},
}
