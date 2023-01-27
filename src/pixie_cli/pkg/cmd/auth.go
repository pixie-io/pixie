/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package cmd

import (
	"github.com/lestrrat-go/jwx/jwt"
	"github.com/segmentio/analytics-go/v3"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	srvutils "px.dev/pixie/src/shared/services/utils"
)

func init() {
	AuthCmd.AddCommand(LoginCmd)

	LoginCmd.PersistentFlags().Bool("manual", false, "Don't automatically open the browser")
	LoginCmd.Flags().Bool("use_api_key", false, "Use API key for authentication")
	LoginCmd.Flags().String("api_key", "", "Use specified API key for authentication.")
}

// AuthCmd is the auth sub-command of the CLI.
var AuthCmd = &cobra.Command{
	Use:   "auth",
	Short: "Authenticate with Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		utils.Info("Nothing here... Please execute one of the subcommands")
		cmd.Help()
	},
}

// LoginCmd is the Login sub-command of Auth.
var LoginCmd = &cobra.Command{
	Use:   "login",
	Short: "Login to Pixie",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("manual", cmd.PersistentFlags().Lookup("manual"))
		viper.BindPFlag("use_api_key", cmd.Flags().Lookup("use_api_key"))
		viper.BindPFlag("api_key", cmd.Flags().Lookup("api_key"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		l := auth.PixieCloudLogin{
			ManualMode: viper.GetBool("manual"),
			CloudAddr:  viper.GetString("cloud_addr"),
			UseAPIKey:  viper.GetBool("use_api_key"),
			APIKey:     viper.GetString("api_key"),
		}
		var refreshToken *auth.RefreshToken
		var err error
		if refreshToken, err = l.Run(); err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to login")
		}
		if err = auth.SaveRefreshToken(refreshToken); err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to persist auth token")
		}

		if token, _ := jwt.Parse([]byte(refreshToken.Token)); token != nil {
			userID := srvutils.GetUserID(token)
			if userID != "" {
				// Associate UserID with AnalyticsID.
				_ = pxanalytics.Client().Enqueue(&analytics.Alias{
					UserId:     pxconfig.Cfg().UniqueClientID,
					PreviousId: userID,
				})
			}
		}
		utils.Info("Authentication Successful")
	},
}
