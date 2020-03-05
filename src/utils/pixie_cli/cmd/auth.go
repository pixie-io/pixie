package cmd

import (
	"github.com/dgrijalva/jwt-go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"gopkg.in/segmentio/analytics-go.v3"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/auth"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"
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

// LoginCmd is the Login sub-command of Auth.
var LoginCmd = &cobra.Command{
	Use:   "login",
	Short: "Login to Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		l := auth.PixieCloudLogin{
			Site:       viper.GetString("site"),
			ManualMode: viper.GetBool("manual"),
			CloudAddr:  viper.GetString("cloud_addr"),
		}
		refreshToken := &auth.RefreshToken{}
		var err error
		if refreshToken, err = l.Run(); err != nil {
			log.WithError(err).Fatal("Failed to login")
		}
		if err = auth.SaveRefreshToken(refreshToken); err != nil {
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
	},
}

func init() {
	AuthCmd.AddCommand(LoginCmd)

	AuthCmd.PersistentFlags().String("site", "", "The site to login to, ex: <company>.withpixie.ai")
	viper.BindPFlag("site", AuthCmd.PersistentFlags().Lookup("site"))
	AuthCmd.MarkPersistentFlagRequired("site")

	AuthCmd.PersistentFlags().Bool("manual", false, "Don't automatically open the browser")
	viper.BindPFlag("manual", AuthCmd.PersistentFlags().Lookup("manual"))
}
