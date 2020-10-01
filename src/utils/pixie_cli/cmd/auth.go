package cmd

import (
	"github.com/dgrijalva/jwt-go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/components"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/auth"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"
	cliLog "pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/utils"
)

func init() {
	AuthCmd.AddCommand(LoginCmd)

	AuthCmd.PersistentFlags().Bool("manual", false, "Don't automatically open the browser")
	viper.BindPFlag("manual", AuthCmd.PersistentFlags().Lookup("manual"))

	AuthCmd.PersistentFlags().String("org_name", "", "Select ORG to login into")
	viper.BindPFlag("org_name", AuthCmd.PersistentFlags().Lookup("org_name"))
	AuthCmd.PersistentFlags().MarkHidden("org_name")
}

// AuthCmd is the auth sub-command of the CLI.
var AuthCmd = &cobra.Command{
	Use:   "auth",
	Short: "Authenticate with Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		cliLog.Info("Nothing here... Please execute one of the subcommands")
		cmd.Help()
		return
	},
}

// LoginCmd is the Login sub-command of Auth.
var LoginCmd = &cobra.Command{
	Use:   "login",
	Short: "Login to Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		orgName := viper.GetString("org_name")
		l := auth.PixieCloudLogin{
			ManualMode: viper.GetBool("manual"),
			CloudAddr:  viper.GetString("cloud_addr"),
			OrgName:    orgName,
		}
		refreshToken := &auth.RefreshToken{}
		var err error
		if refreshToken, err = l.Run(); err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to login")
		}
		if err = auth.SaveRefreshToken(refreshToken); err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to persists auth token")
		}

		if token, _ := jwt.Parse(refreshToken.Token, nil); token != nil {
			sc, ok := token.Claims.(jwt.MapClaims)
			if ok {
				userID, _ := sc["UserID"].(string)
				// Associate UserID with AnalyticsID.
				_ = pxanalytics.Client().Enqueue(&analytics.Alias{
					UserId:     pxconfig.Cfg().UniqueClientID,
					PreviousId: userID,
				})
			}
		}
		cliLog.Info("Authentication Successful")

		if orgName != "" {
			components.RenderBureaucratDragon(orgName)
		}
	},
}
