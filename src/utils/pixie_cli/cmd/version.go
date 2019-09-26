package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
	"pixielabs.ai/pixielabs/src/shared/version"
)

// NewCmdVersion creates a new "version" command.
func NewCmdVersion() *cobra.Command {
	return &cobra.Command{
		Use:   "version",
		Short: "Print the version number of the cli",
		Run: func(cmd *cobra.Command, args []string) {
			fmt.Printf("%s\n", version.GetVersion().ToString())
		},
	}
}
