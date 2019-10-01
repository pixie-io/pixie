package cmd

import (
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/cmd/auth"
)

// AuthCmd is the auth sub-command of the CLI.
var AuthCmd = &cobra.Command{
	Use:   "auth",
	Short: "Authenticate with Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		log.Info("Nothing here... Please execute one of the subcommands")
		cmd.Help()
		return
	},
}

func init() {
	AuthCmd.AddCommand(auth.LoginCmd)

	AuthCmd.PersistentFlags().String("site", "", "The site to login to, ex: <company>.withpixie.ai")
	viper.BindPFlag("site", AuthCmd.PersistentFlags().Lookup("site"))
	AuthCmd.MarkPersistentFlagRequired("site")

	AuthCmd.PersistentFlags().Bool("manual", false, "Don't automatically open the browser")
	viper.BindPFlag("manual", AuthCmd.PersistentFlags().Lookup("manual"))
}
