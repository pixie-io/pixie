package commands

import (
	"github.com/spf13/cobra"
)

// PixieCmd is a cobra CLI utility to run smoke-tests on client machines to determine validity for agents.
var PixieCmd = &cobra.Command{
	Use:     "smoke-test",
	Version: `0.1`,
	Short:   "smoke-test is a utility to validate compute environment to run pixie agents",
	Long: `A command line utility to validate whether ` +
		`pixie agents can run successfully to generate system logs and network usage.`,
	Run: func(cmd *cobra.Command, args []string) {
		cmd.HelpFunc()(cmd, args)
	},
}

func init() {
	PixieCmd.SetVersionTemplate("Pixie smoke-test cli version: {{.Version}}\n")
}
